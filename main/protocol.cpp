#include <map>
#include <inttypes.h>
#include "protocol.hpp"
#include "handlers.hpp"
#include "driver/uart.h"
#include "crc.hpp"

void uart_write(uint8_t *buf, size_t len)
{
    uart_write_bytes(UART_PORT, buf, len);
}

bool uart_read(uint8_t *buf, size_t len, bool no_auto_ma = false)
{
    int err = uart_read_bytes(UART_PORT, buf, len, RX_TIMEOUT);
    if (!no_auto_ma && err < 0)
    {
        SEND_MR(0xFF, MR_FATAL);
        return false;
    }

    return !no_auto_ma;
}

void uart_protocol_read_v1()
{
    // Init calculated CRC with already read magic number
    uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_BASE_PACKET};
    uint8_t crc = crc8(0x00, magics, 2);

    // Read packet ID
    uint8_t id;
    uart_read(&id, 1);
    crc = crc8(crc, &id, 1);

    // Get packet handler from id
    packetHandler handler = uart_get_handler_from_id(id);
    if (handler.packetFinished == nullptr)
    {
        // This means that ID was not found an handle
        SEND_MR(id, MR_FATAL);
        return;
    }

    // Read body length
    uint16_t bodylen;
    uart_read((uint8_t *)&bodylen, 2);

    // Avoid bodies too large for the little memory
    if (bodylen > RX_BODYSIZE)
    {
        SEND_MR(id, MR_FATAL);
        return;
    }
    crc = crc8(crc, (uint8_t *)&bodylen, 2);

    // Read number of followups
    uint32_t following;
    uart_read((uint8_t *)&following, 4);
    crc = crc8(crc, (uint8_t *)&following, 4);

    // Read body
    uint8_t bodybuf[RX_BODYSIZE];
    uart_read(bodybuf, bodylen);
    crc = crc8(crc, bodybuf, bodylen);

    // Check CRC
    uint8_t rcrc;
    uart_read(&rcrc, 1);
    if (crc != rcrc)
    {
        SEND_MR(id, MR_FAIL); // Softfail, because a incorrect crc can be caused by a transport problem.
        return;
    }

    // Handle body
    void *ctx = handler.firstBufferHandler(bodybuf, bodylen);

    // End of the base packet
    SEND_MR(id, MR_SUCCESS);

    // Receive followup packets
    // When receiving them, already alocated memory values will be reused for memory efficiency
    for (size_t i = 1; i < following + 1; i++) // "i" is 1-based, because if the first followup packet is retried, he will go is initial value minus 1 for a bit
    {
        // Magic numbers
        uint8_t magic1;
        uart_read(&magic1, 1);
        uint8_t magic2;
        uart_read(&magic2, 1);

        if (magic1 != MAGICNUMBER_V1 || magic2 != MAGICNUMBER_FOLLOWUP_PACKET)
        {
            SEND_MR(id, MR_FATAL);
            return;
        }

        // Restart CRC calculation for this followup packet
        crc = crc8(0x00, &magic1, 1);
        crc = crc8(crc, &magic2, 1);

        // Read body length
        uart_read((uint8_t *)&bodylen, 2);

        // Avoid bodies too large for the little memory
        if (bodylen > RX_BODYSIZE)
        {
            SEND_MR(id, MR_FATAL);
            handler.packetFinished(ctx, true);
            return;
        }
        crc = crc8(crc, (uint8_t *)&bodylen, 2);

        // Read body
        uart_read((uint8_t *)&bodybuf, bodylen);
        crc = crc8(crc, (uint8_t *)&bodybuf, bodylen);

        // Check CRC
        uint8_t rcrc;
        uart_read(&rcrc, 1);
        if (crc != rcrc)
        {
            SEND_MR(id, MR_FAIL);
            i--; // iteration must be redone.
            continue;
        }

        // Handle body
        handler.followingBufferHandler(ctx, bodybuf, bodylen);

        // End of the followup packet
        SEND_MR(id, MR_SUCCESS);
    }

    // End of the packet chain.
    handler.packetFinished(ctx, false);
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
                uart_protocol_read_v1();
            else if (magicnum2 != MAGICNUMBER_MR)
            {
                // Invalid request.
                // Microresponses and followup packets shouldnt be handled here.
                // And other garbage should not too.
                // However, microresponses shouldnt be answered by another microresponse.
                SEND_MR(0xFF, MR_FATAL);
            }
        }

        // To avoid confusion with other stuff running on the line, invalid magic value in the first byte should be ignored.
    }
}

bool uart_protocol_write_base_v1(uint8_t id, uint32_t following, uint8_t *body, uint16_t bodylen)
{
    while (1) // Use a loop in case we need to resend it when a softfail occurs
    {
        uint8_t crc;

        uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_BASE_PACKET};
        uart_write(magics, 2);
        crc = crc8(0x00, magics, 2);

        uart_write(&id, 1);
        crc = crc8(crc, &id, 1);

        uart_write((uint8_t *)&bodylen, 2);
        crc = crc8(crc, (uint8_t *)&bodylen, 2);

        uart_write((uint8_t *)&following, 4);
        crc = crc8(crc, (uint8_t *)&following, 4);

        uart_write(body, bodylen);
        crc = crc8(crc, body, bodylen);

        uart_write(&crc, 1);

        // Receive microresponse
        uint8_t rmr[4];
        uart_read(rmr, 4);
        if (rmr[0] == MAGICNUMBER_V1 && rmr[1] == MAGICNUMBER_MR && rmr[2] == id)
        {
            if (rmr[3] == MR_SUCCESS)
                return true;
            else if (rmr[3] == MR_FAIL)
                continue;
        }

        // Will trigger if any byte is incorrect, or MR_FATAL is received
        return false;
    }
}
bool uart_protocol_write_followup_v1(uint8_t id, uint8_t *body, uint16_t bodylen)
{
    while (1) // Use a loop in case we need to resend it when a softfail occurs
    {
        uint8_t crc;

        uint8_t magics[2] = {MAGICNUMBER_V1, MAGICNUMBER_FOLLOWUP_PACKET};
        uart_write(magics, 2);
        crc = crc8(0x00, magics, 2);

        uart_write((uint8_t *)&bodylen, 2);
        crc = crc8(crc, (uint8_t *)&bodylen, 2);

        uart_write(body, bodylen);
        crc = crc8(crc, body, bodylen);

        uart_write(&crc, 1);

        // Receive microresponse
        uint8_t rmr[4];
        uart_read(rmr, 4);
        if (rmr[0] == MAGICNUMBER_V1 && rmr[1] == MAGICNUMBER_MR && rmr[2] == id)
        {
            if (rmr[3] == MR_SUCCESS)
                return true;
            else if (rmr[3] == MR_FAIL)
                continue;
        }

        // Will trigger if any byte is incorrect, or MR_FATAL is received
        return false;
    }
}

TaskHandle_t uart_init()
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
    TaskHandle_t task;
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, NULL, configMAX_PRIORITIES - 1, &task);

    return task;
}