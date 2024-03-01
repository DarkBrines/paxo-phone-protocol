#include <stdio.h>
#include <inttypes.h>
#include "crc.hpp"

uint8_t crc8(uint8_t crc, uint8_t *data, size_t len)
{
    size_t i, j;
    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ CRC8_POLYNOMIAL);
            else
                crc <<= 1;
        }
    }

    return crc;
}