import serial
from serial import Serial
import crc
import logging
import time

crc8 = crc.Calculator(crc.Crc8.CCITT)
crc32 = crc.Calculator(crc.Crc32.CRC32)

LATEST_VERSION = 1


class PacketType:
    METADATA_REQ = 0x00
    METADATA_RES = 0x01
    CHECK_APP_REQ = 0x02
    CHECK_APP_RES = 0x03
    LIST_APPS_REQ = 0x04
    LIST_APPS_RES = 0x05

    INSTALL_APP_REQ = 0x10
    INSTALL_APP_RES = 0x11
    END_INSTALL = 0x12
    END_INSTALL_STATUS = 0x13
    REMOVE_APP_REQ = 0x14
    REMOVE_APP_RES = 0x15

    FILE_UPLOAD = 0x60
    FILE_UPLOAD_DONE = 0x61
    FILE_DOWNLOAD = 0x62
    FILE_DOWNLOAD_DATA = 0x63

    PROTOCOL_VERSION_REQ = 0x7C
    PROTOCOL_VERSION_RES = 0x7D
    ANNOUNCE_REVERSE_COMPATIBILITY = 0x7F

class Microresponses:
    SUCCESS = 0xA0
    FAIL = 0xA1
    FATAL = 0xA2

class Connection:
    ser: Serial
    version: int

    def __init__(self, ser: Serial):
        self.ser = ser

    def send(*args):
        raise Exception("Method not implemented")

    def recv(*args):
        raise Exception("Method not implemented")


class ConnectionV1(Connection):
    version = 1
    tridCounter = 0

    def sendRaw(self, data: bytes):
        print(f"Writing to UART: {data.hex()}")
        self.ser.write(data)

    def recvRaw(self, size) -> bytes:
        print(f"Reading {size} bytes from UART...")
        res = self.ser.read(size)
        print(f"Reading from UART: {res.hex()}")
        return res

    def send(self, id: int, data: bytes):
        following = len(data) // 2048
        lastPacketSize = len(data) % 2048

        if lastPacketSize == 0 and following != 0:
            following -= 1
            lastPacketSize = 2048

        if following == 0:
            self.__sendBasePacket(id, data, 0)
        else:
            trid = self.__sendBasePacket(id, data[:2048], following)
            for i in range(following):
                if i != len(following)-1:
                    self.__sendFollowupPacket(data[(2048 * i):(2048 * (i+1))], trid)
                else: # Last one
                    self.__sendFollowupPacket(data[(2048 * i):(2048 * i + lastPacketSize)], trid)


    def __sendBasePacket(self, msgid: int, data: bytes, following: int) -> int:
        self.tridCounter += 1
        trid = self.tridCounter
        while True:
            payload = bytes([0x91, 0xc1])
            payload += trid.to_bytes(2, "little", signed=False)
            payload += msgid.to_bytes(1, "little", signed=False)
            payload += following.to_bytes(4, "little", signed=False)
            payload += len(data).to_bytes(2, "little", signed=False)
            payload += data
            payload += crc8.checksum(payload).to_bytes(1, "little")

            self.sendRaw(payload)

            mr = self.__recvMicroresponse()
            if mr == Microresponses.FAIL:
                continue
            break


    def __sendFollowupPacket(self, data: bytes, trid: int):
        while True:
            payload = bytes([0x91, 0xc2])
            payload += trid.to_bytes(2, "little", signed=False)
            payload += len(data).to_bytes(2, "little", signed=False)
            payload += data
            payload += crc8.checksum(payload).to_bytes(1, "little")

            self.sendRaw(payload)

            mr = self.__recvMicroresponse()
            if mr == Microresponses.FAIL:
                continue
            break


    def __sendMicroresponse(self, trid: int, status: int):
        payload = bytes([0x91, 0xc3])
        payload += trid.to_bytes(2, "little", signed=False)
        payload += status.to_bytes(1, "little", signed=False)

        self.sendRaw(payload)


    def recv(self, expectedID: int) -> bytes:
        # Receive base packet
        trid, _, following, body = self.__recvBasePacket(expectedID)

        # Receive followup packets
        for _ in range(following):
            body += self.__recvFollowupPacket(trid)
        
        return body

    def __recvBasePacket(self, expectedID = -1) -> (int, int, int, bytes):
        while True:
            if self.recvRaw(2) != bytes([0x91, 0xc1]):
                self.__sendMicroresponse(0xff, Microresponses.FATAL)
                raise Exception("Unexpected magic numbers received")

            trid = int.from_bytes(self.recvRaw(2), "little")

            # If expectedID is not specified, any packet will be received
            msgid = self.recvRaw(1)[0]
            if expectedID != -1 and msgid != expectedID:
                continue

            following = int.from_bytes(self.recvRaw(4), "little")
            bodylen = int.from_bytes(self.recvRaw(2), "little")
            body = self.recvRaw(bodylen)

            # Check CRC
            crc = crc8.checksum(bytes(
                bytes([0x91, 0xc1])
                + trid.to_bytes(2, "little")
                + msgid.to_bytes(1, "little") 
                + following.to_bytes(4, "little")
                + bodylen.to_bytes(2, "little") 
                + body))
        
            if self.recvRaw(1)[0] != crc:
                self.__sendMicroresponse(trid, Microresponses.FAIL)
                continue

            self.__sendMicroresponse(trid, Microresponses.SUCCESS)
            return (trid, msgid, following, body)

    
    def __recvFollowupPacket(self, trid: int) -> bytes:
        while True:
            if self.recvRaw(2) != bytes([0x91, 0xc2]):
                self.__sendMicroresponse(0xff, Microresponses.FATAL)
                raise Exception("Unexpected magic numbers received")

            rtrid = int.from_bytes(self.recvRaw(2), "little")
            if rtrid != trid:
                raise Exception("Unexpected transmission ID received")

            bodylen = int.from_bytes(self.recvRaw(2), "little")
            body = self.recvRaw(bodylen)

            # Check CRC
            crc = crc8.checksum(bytes(
                [0x91, 0xc2]
                + rtrid.to_bytes(2, "little")
                + bodylen.to_bytes(2, "little") 
                + body))
        
            if self.recvRaw(1)[0] != crc:
                self.__sendMicroresponse(trid, Microresponses.FAIL)
                continue

            self.__sendMicroresponse(trid, Microresponses.SUCCESS)
            return body


    def __recvMicroresponse(self) -> int:
        if self.recvRaw(2) != bytes([0x91, 0xc3]):
            raise Exception("Unexpected magic numbers received")

        trid = self.recvRaw(2)[0]
        status = self.recvRaw(1)[0]

        if status == Microresponses.FATAL:
            raise Exception(f"A fatal error occured while the transmission {id}.")
        elif status == Microresponses.FAIL:
            logging.warning(f"A softfail error occured while the transmission {id}. The packet will be resent.")

        return status


    def queryProtocolVersion(self) -> int:
        self.send(PacketType.PROTOCOL_VERSION_REQ, bytes([]))
        response = self.recv(PacketType.PROTOCOL_VERSION_RES)

        return response[0]

def paxoConnect(port: str) -> Connection:
    # Connect to serial
    ser = Serial(
        port,
        baudrate=115200,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        rtscts=True
    )

    # Temporarly assigns this one
    connV1 = ConnectionV1(ser)

    # Send a protocol version request
    connV1.send(PacketType.PROTOCOL_VERSION_REQ, bytes())
    # Get the response
    data = connV1.recv(PacketType.PROTOCOL_VERSION_RES)
    # Get version from response
    vernumber = data[0]

    if vernumber == 1:
        return connV1

    # if vernumber == 2:
    #    return ConnectionV2(ser)
    # etc...

    else:  # If the host isn't compatible with the device, version 1 must be used
        logging.warning(
            "Device protocol is newer than the host protocol. For compatibility reasons, protocol v1 will be used. Some features will not be available."
        )
        return connV1


def main():
    conn = paxoConnect("/dev/ttyACM0")
    print("Hey! I'm using version", conn.version)

if __name__ == "__main__":
    main()
