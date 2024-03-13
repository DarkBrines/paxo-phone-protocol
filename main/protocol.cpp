#include <map>
#include <inttypes.h>
#include "protocol.hpp"
#include "rxhandlers.hpp"
#include "txhandlers.hpp"
#include "driver/uart.h"
#include "crc.hpp"
#include "freertos/queue.h"

void uart_write(uint8_t *buf, size_t len)
{
    uart_write_bytes(UART_PORT, buf, len);
}

uint16_t txTransmissionCounter;
std::map<uint16_t, txTransmission> txTransmissions;

void uart_protocol_write_base_v1(uint16_t trid, uint8_t msgid, uint32_t following, uint8_t *body, uint16_t bodylen)
{
    uint8_t crc;

    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_BASE_PACKET};
    uart_write(magics, 2);
    crc = crc8(0x00, magics, 2);

    uart_write((uint8_t *)&trid, 2);
    crc = crc8(crc, (uint8_t *)&trid, 2);

    uart_write(&msgid, 1);
    crc = crc8(crc, &msgid, 1);

    uart_write((uint8_t *)&following, 4);
    crc = crc8(crc, (uint8_t *)&following, 4);

    uart_write((uint8_t *)&bodylen, 2);
    crc = crc8(crc, (uint8_t *)&bodylen, 2);

    uart_write(body, bodylen);
    crc = crc8(crc, body, bodylen);

    uart_write(&crc, 1);
}

void uart_protocol_write_followup_v1(uint16_t trid, uint8_t *body, uint16_t bodylen)
{
    uint8_t crc;

    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_FOLLOWUP_PACKET};
    uart_write(magics, 2);
    crc = crc8(0x00, magics, 2);

    uart_write((uint8_t *)&trid, 2);
    crc = crc8(crc, (uint8_t *)&trid, 2);

    uart_write((uint8_t *)&bodylen, 2);
    crc = crc8(crc, (uint8_t *)&bodylen, 2);

    uart_write(body, bodylen);
    crc = crc8(crc, body, bodylen);

    uart_write(&crc, 1);
}

void uart_protocol_write_mr_v1(uint16_t trid, uint8_t status)
{
    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_MR};
    uart_write(magics, 2);

    uart_write((uint8_t *)&trid, 2);

    uart_write(&status, 1);
}

void uart_protocol_start_transmission(uint8_t msgid, transmissionTxHandler handler, void *ctx, uint16_t followups)
{
    // Get a transmission ID
    txTransmissionCounter++;
    uint16_t trid = txTransmissionCounter;

    txTransmission tr = {
        .handler = handler,
        .ctx = ctx,
        .following = followups,
        .successful = 0,
    };

    txTransmissions[trid] = tr;

    // Send first body part here in the base packet
    uint8_t buf[TX_BODYSIZE];
    uint16_t buflen = handler.getFirstBuffer(ctx, buf);

    uart_protocol_write_base_v1(trid, msgid, followups, buf, buflen);
}

std::map<uint16_t, rxTransmission> rxTransmissions;

bool uart_read(uint8_t *buf, size_t len, bool no_auto_ma = false)
{
    int err = uart_read_bytes(UART_PORT, buf, len, RX_TIMEOUT);
    if (!no_auto_ma && err < 0)
    {
        uart_protocol_write_mr_v1(0xFF, MR_FATAL);
        return false;
    }

    return !no_auto_ma;
}

void uart_protocol_read_base_v1()
{
    // Init calculated CRC with already read magic number
    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_BASE_PACKET};
    uint8_t crc = crc8(0x00, magics, 2);

    // Read transmission ID
    uint16_t trid;
    uart_read((uint8_t *)&trid, 2);
    crc = crc8(crc, (uint8_t *)&trid, 2);

    // Read message ID
    uint8_t msgid;
    uart_read(&msgid, 1);
    crc = crc8(crc, &msgid, 1);

    // Get packet handler from msgid
    transmissionRxHandler handler = uart_get_handler_from_msgid(msgid);
    if (handler.transmissionFinished == nullptr)
    {
        // This means that ID was not found an handle
        uart_protocol_write_mr_v1(trid, MR_FATAL);
        return;
    }

    // Read number of followups
    uint32_t following;
    uart_read((uint8_t *)&following, 4);
    crc = crc8(crc, (uint8_t *)&following, 4);

    // Read body length
    uint16_t bodylen;
    uart_read((uint8_t *)&bodylen, 2);
    crc = crc8(crc, (uint8_t *)&bodylen, 2);

    // Avoid bodies too large for the little memory
    if (bodylen > RX_BODYSIZE)
    {
        uart_protocol_write_mr_v1(trid, MR_FATAL);
        return;
    }

    // Read body
    uint8_t bodybuf[bodylen];
    uart_read(bodybuf, bodylen);
    crc = crc8(crc, bodybuf, bodylen);

    // Check CRC
    uint8_t rcrc;
    uart_read(&rcrc, 1);
    if (crc != rcrc)
    {
        uart_protocol_write_mr_v1(trid, MR_FAIL); // Softfail, because an incorrect crc can be caused by a transport problem. Must retry
        return;
    }

    // Handle body
    void *ctx = handler.firstBufferHandler(bodybuf, bodylen);

    // End of the base packet
    uart_protocol_write_mr_v1(trid, MR_SUCCESS);

    // Add to handlers, or finish here
    if (following == 0)
    {
        handler.transmissionFinished(ctx, false);
    }
    else
    {
        rxTransmission tr = {
            .handler = handler,
            .ctx = ctx,
            .following = following,
        };
        rxTransmissions[trid] = tr;
    }
}

void uart_protocol_read_followup_v1()
{
    // Init calculated CRC with already read magic number
    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_FOLLOWUP_PACKET};
    uint8_t crc = crc8(0x00, magics, 2);

    // Read transmission ID
    uint16_t trid;
    uart_read((uint8_t *)&trid, 2);
    crc = crc8(crc, (uint8_t *)&trid, 2);

    // Get transmission from ID
    if (rxTransmissions.find(trid) == rxTransmissions.end())
    {
        uart_protocol_write_mr_v1(trid, MR_FATAL);
        return;
    }
    rxTransmission tr = rxTransmissions[trid];

    // Read body length
    uint16_t bodylen;
    uart_read((uint8_t *)&bodylen, 2);
    crc = crc8(crc, (uint8_t *)&bodylen, 2);

    // Avoid bodies too large for the little memory
    if (bodylen > RX_BODYSIZE)
    {
        uart_protocol_write_mr_v1(trid, MR_FATAL);
        return;
    }

    // Read body
    uint8_t bodybuf[bodylen];
    uart_read(bodybuf, bodylen);
    crc = crc8(crc, bodybuf, bodylen);

    // Check CRC
    uint8_t rcrc;
    uart_read(&rcrc, 1);
    if (crc != rcrc)
    {
        uart_protocol_write_mr_v1(trid, MR_FAIL); // Softfail, because an incorrect crc can be caused by a transport problem. Must retry
        return;
    }

    // From here, success
    tr.handler.followingBufferHandler(tr.ctx, bodybuf, bodylen);
    tr.following--;

    if (tr.following == 0)
    {
        tr.handler.transmissionFinished(tr.ctx, false);
        rxTransmissions.erase(trid);
    }
    else
    {
        // Save the decremental
        rxTransmissions[trid] = tr;
    }
}

// When this function is triggered, it means a microresponse
// has been received. This function runtime will also take care
// of calling back the related txTransmission and sending
// the following packets
void uart_protocol_read_mr_v1()
{
    // Read transmission ID
    uint16_t trid;
    uart_read((uint8_t *)&trid, 2);

    // Get transmission from ID
    if (txTransmissions.find(trid) == txTransmissions.end())
    {
        return; // Don't send a microresponse in response to a microresponse ;)
    }
    txTransmission tr = txTransmissions[trid];

    // Read status
    uint8_t status;
    uart_read(&status, 1);

    if (status == MR_FATAL)
    {
        txTransmissions.erase(trid);
        return;
    }
    else if (status == MR_SUCCESS)
    {
        tr.successful++;
        if (tr.successful >= tr.following + 1) // tr.successful also includes the base packet, that's why the +1
        {
            tr.handler.transmissionFinished(tr.ctx);
            txTransmissions.erase(trid);
            return;
        }
        txTransmissions[trid] = tr; // Save the tr.successful increment
    }

    // Arriving here means that the state is a success or a softfail, and that there is still followups to print
    uint8_t buf[TX_BODYSIZE];
    uint16_t buflen = tr.handler.getFollowupBuffer(tr.ctx, buf, tr.successful);

    uart_protocol_write_followup_v1(trid, buf, buflen);
}

void uart_rx_task(void *arg)
{
    while (1)
    {
        uint8_t magicnum1;
        // Don't convert this function to uart_read, because this one have an infinite delay.
        uart_read_bytes(UART_PORT, &magicnum1, 1, portMAX_DELAY);

        if (magicnum1 == MAGICNUMBER_V1)
        {
            uint8_t magicnum2;
            uart_read(&magicnum2, 1);

            if (magicnum2 == MAGICNUMBER_BASE_PACKET)
                uart_protocol_read_base_v1();

            else if (magicnum2 == MAGICNUMBER_FOLLOWUP_PACKET)
                uart_protocol_read_followup_v1();

            else if (magicnum2 == MAGICNUMBER_MR)
                uart_protocol_read_mr_v1();
        }

        // To avoid confusion with other stuff running on the line, invalid magic value in the first byte should be ignored.
    }
}

TaskHandle_t rxTask;

void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_PORT, RX_BODYSIZE * 2, TX_BODYSIZE * 2, 0, NULL, 0);
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    // Create RX task to receive packets
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &rxTask);

    txTransmissionCounter = 0;
}

void uart_free()
{
    vTaskDelete(rxTask);
    rxTransmissions.clear();
    txTransmissions.clear();
}