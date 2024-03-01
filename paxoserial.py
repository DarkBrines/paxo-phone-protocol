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
            self.__sendBasePacket(id, data[:2048], following)
            for i in range(following):
                if i != len(following)-1:
                    self.__sendFollowupPacket(data[(2048 * i):(2048 * (i+1))])
                else: # Last one
                    self.__sendFollowupPacket(data[(2048 * i):(2048 * i + lastPacketSize)])


    def __sendBasePacket(self, id: int, data: bytes, following: int):
        while True:
            payload = bytes([0x91, 0xc1])
            payload += id.to_bytes(1, "little", signed=False)
            payload += len(data).to_bytes(2, "little", signed=False)
            payload += following.to_bytes(4, "little", signed=False)
            payload += data
            payload += crc8.checksum(payload).to_bytes(1, "little")

            self.sendRaw(payload)

            mr = self.__recvMicroresponse()
            if mr == Microresponses.FAIL:
                continue
            break


    def __sendFollowupPacket(self, data: bytes):
        while True:
            payload = bytes([0x91, 0xc2])
            payload += len(data).to_bytes(2, "little", signed=False)
            payload += data
            payload += crc8.checksum(payload).to_bytes(1, "little")

            self.sendRaw(payload)

            mr = self.__recvMicroresponse()
            if mr == Microresponses.FAIL:
                continue
            break


    def __sendMicroresponse(self, id: int, status: int):
        payload = bytes([0x91, 0xc3])
        payload += id.to_bytes(1, "little", signed=False)
        payload += status.to_bytes(1, "little", signed=False)

        self.sendRaw(payload)


    def recv(self, expectedID: int) -> bytes:
        # Receive base packet
        body, following = self.__recvBasePacket(expectedID)

        # Receive followup packets
        for _ in range(following):
            body += self.__recvFollowupPacket()
        
        return body

    def __recvBasePacket(self, expectedID: int) -> (bytes, int):
        while True:
            if self.recvRaw(2) != bytes([0x91, 0xc1]):
                self.__sendMicroresponse(0xff, Microresponses.FATAL)
                raise Exception("Unexpected magic numbers received")

            if self.recvRaw(1) != expectedID.to_bytes(1, "little"):
                self.__sendMicroresponse(0xff, Microresponses.FATAL)
                raise Exception("Unexpected packet ID received")

            bodylen = int.from_bytes(self.recvRaw(2), "little")
            following = int.from_bytes(self.recvRaw(4), "little")
            body = self.recvRaw(bodylen)

            # Check CRC
            crc = crc8.checksum(bytes(
                bytes([0x91, 0xc1])
                + expectedID.to_bytes(1, "little") 
                + bodylen.to_bytes(2, "little") 
                + following.to_bytes(4, "little")
                + body))
        
            if self.recvRaw(1)[0] != crc:
                self.__sendMicroresponse(expectedID, Microresponses.FAIL)
                continue

            self.__sendMicroresponse(expectedID, Microresponses.SUCCESS)
            return (body, following)

    
    def __recvFollowupPacket(self) -> bytes:
        while True:
            if self.recvRaw(2) != bytes([0x91, 0xc2]):
                self.__sendMicroresponse(0xff, Microresponses.FATAL)
                raise Exception("Unexpected magic numbers received")

            bodylen = int.from_bytes(self.recvRaw(2), "little")
            body = self.recvRaw(bodylen)

            # Check CRC
            crc = crc8.checksum(bytes(
                [0x91, 0xc2] 
                + bodylen.to_bytes(2, "little") 
                + body))
        
            if self.recvRaw(1)[0] != crc:
                self.__sendMicroresponse(expectedID, Microresponses.FAIL)
                continue

            self.__sendMicroresponse(expectedID, Microresponses.SUCCESS)
            return body


    def __recvMicroresponse(self) -> int:
        if self.recvRaw(2) != bytes([0x91, 0xc3]):
            time.sleep(5)
            print(self.ser.read_all().hex())
            raise Exception("Unexpected magic numbers received")

        id = self.recvRaw(1)[0]
        status = self.recvRaw(1)[0]

        if status == Microresponses.FATAL:
            raise Exception(f"A fatal error occured while transporting packet of ID {id}")
        elif status == Microresponses.FAIL:
            logging.warning(f"A softfail error occured while transporting packet of ID {id}. The packet will be resent.")

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
    print("I'm using version", conn.version)

if __name__ == "__main__":
    main()
