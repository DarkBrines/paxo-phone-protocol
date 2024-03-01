#include "keyboard.hpp"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const gpio_num_t matrix_o[4] = {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3};
static const gpio_num_t matrix_i[4] = {GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_4, GPIO_NUM_5};

uint16_t scan_matrix()
{
    uint16_t res = 0x0000;

    for (uint8_t o = 0; o < 4; o++)
    {
        gpio_set_level(matrix_o[o], 0);

        for (uint8_t i = 0; i < 4; i++)
        {
            if (gpio_get_level(matrix_i[i]) == 0)
            {
                // Button is pressed
                // o is row, i is column
                res |= (1 << (o * 4 + i));
            }
        }

        gpio_set_level(matrix_o[o], 1);
    }

    return res;
}

char *get_key_from_code(uint16_t code)
{
    if (code & KEYPRESS_0)
        return "0";
    if (code & KEYPRESS_1)
        return "1";
    if (code & KEYPRESS_2)
        return "2";
    if (code & KEYPRESS_3)
        return "3";
    if (code & KEYPRESS_4)
        return "4";
    if (code & KEYPRESS_5)
        return "5";
    if (code & KEYPRESS_6)
        return "6";
    if (code & KEYPRESS_7)
        return "7";
    if (code & KEYPRESS_8)
        return "8";
    if (code & KEYPRESS_9)
        return "9";
    if (code & KEYPRESS_A)
        return "A";
    if (code & KEYPRESS_B)
        return "B";
    if (code & KEYPRESS_C)
        return "C";
    if (code & KEYPRESS_D)
        return "D";
    if (code & KEYPRESS_STAR)
        return "*";
    if (code & KEYPRESS_DIEZ)
        return "#";

    return (char *)0xFF;
}

void wait_for_key_release()
{
    while (scan_matrix() != 0)
    {
        WAIT_A_BIT;
    }
}

void keyboard_init()
{
    for (uint8_t i = 0; i < 4; i++)
    {
        gpio_reset_pin(matrix_o[i]);
        gpio_set_direction(matrix_o[i], GPIO_MODE_OUTPUT);
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        gpio_reset_pin(matrix_i[i]);
        gpio_set_direction(matrix_i[i], GPIO_MODE_INPUT);
    }
}