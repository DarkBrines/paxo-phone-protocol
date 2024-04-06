#include "paxocli.hpp"

#include <stdio.h>
#include <inttypes.h>
#include <cstring>
#include "driver/uart.h"
#include "freertos/task.h"

int paxocli_parse_cmd(char *buf, int bufsize, char *argv[MAX_CMD_COMPONENTS])
{
    if (bufsize <= 1)
        return 0;

    int i = 0;
    int argc = 0;
    if (buf[1] == '~')
    {
        i = 1;
        argc += MAX_CMD_COMPONENTS;
    }

    int compsize = 0;
    bool inbrackets = false;
    while (i < bufsize)
    {
        // Detecte le guillemet de début
        if (compsize == 0 && buf[i] == '"')
            inbrackets = true;

        // Detecte le guillemet de fin
        else if (inbrackets && buf[i] == '"')
        {
            inbrackets = false;

            char fcomp[compsize + 1];
            memcpy(fcomp, buf + i, compsize);
            fcomp[compsize] = 0; // Pour terminer la cstring
            argv[argc] = fcomp;

            compsize = 0;
            argc += 1;
        }

        // Detecte l'espace à la fin
        else if (!inbrackets && buf[i] == ' ')
        {
            // Detecte les espaces qui servent à rien
            if (compsize == 0)
                goto next;

            char fcomp[compsize + 1];
            memcpy(fcomp, buf, compsize);
            fcomp[compsize] = 0; // Pour terminer la cstring
            argv[argc] = fcomp;

            compsize = 0;
            argc++;
        }

        // Pour un caractère régulier entré
        else
        {
            compsize++;
        }

    next:
        i++;
    }

    return argc;
}

bool inputecho = true;

void paxocli_rx_task(void *)
{
    while (1)
    {
        if (inputecho)
        {
            uart_write_bytes(UART_PORT, ">>>", 3);
        }
        else
        {
            uart_write_bytes(UART_PORT, ">!>", 3);
        }

        char buf[MAX_CMD_COMPONENT_SIZE * MAX_CMD_COMPONENTS];
        int bufsize = 0;
        int progmode = -1;

        while (1)
        {
            // Laisse le CPU respirer
            vTaskDelay(200 / portTICK_PERIOD_MS);

            if (bufsize >= sizeof(buf))
                bufsize = 0;

            int read = uart_read_bytes(UART_PORT, buf + bufsize, sizeof(buf) - bufsize, portMAX_DELAY);
            if (read < 0) // Detecte une erreur et risque probablement de faire crash le tel
                return;
            else if (read == 0)
                continue;

            bufsize += read;

            if (buf[bufsize - 1] == '\x00' || buf[bufsize - 1] == '\n')
            {
                // Echo le saut de ligne
                if (buf[bufsize - 1] == '\n')
                    uart_write_bytes(UART_PORT, "\n", 1);

                break;
            }
        }

        char *argv[MAX_CMD_COMPONENTS];
        int argc = paxocli_parse_cmd(buf, bufsize, argv);

        if (argc <= 0)
            continue;

        bool progmode = (argc % MAX_CMD_COMPONENTS >= 1);
        if (progmode)
            argc -= MAX_CMD_COMPONENTS;
    }
}

void paxocli_init()
{
    // Initialise UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_PORT, 2048, 2048, 0, NULL, 0);
    // Configure les paramètres UART
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    TaskHandle_t task;
    xTaskCreate(paxocli_rx_task, "cli-rx", 2048, NULL, configMAX_PRIORITIES - 1, &task);
}