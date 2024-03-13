#include <stdio.h>
#include "protocol.hpp"
#include "crc.hpp"
#include "keyboard.hpp"
#include "freertos/task.h"

extern "C" void app_main()
{
    uart_init();
    keyboard_init();

    printf("Phone booted\n");

    while ((scan_matrix() & KEYPRESS_D) == 0)
    {
        WAIT_A_BIT;
    }

    wait_for_key_release();

    printf("Shutting down...\n");

    // Artificial shutdown delay
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}