#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT UART_NUM_0
#define RX_BODYSIZE 1024 * 2
#define TX_BODYSIZE RX_BODYSIZE
#define RX_TIMEOUT 10000 / portTICK_PERIOD_MS

#define UART_PROTOCOL_LATEST_VERSION MAGICNUMBER_V1

#define MAGICNUMBER_V1 0x91
#define MAGICNUMBER_BASE_PACKET 0xC1
#define MAGICNUMBER_FOLLOWUP_PACKET 0xC2
#define MAGICNUMBER_MR 0xC3

#define MR_SUCCESS 0xA0
#define MR_FAIL 0xA1
#define MR_FATAL 0xA2

#define METADATA_REQ 0x00
#define METADATA_RES 0x01
#define CHECK_APP_REQ 0x02
#define CHECK_APP_RES 0x03
#define LIST_APPS_REQ 0x04
#define LIST_APPS_RES 0x05
#define INSTALL_APP_REQ 0x10
#define INSTALL_APP_RES 0x11
#define END_INSTALL 0x12
#define END_INSTALL_STATUS 0x13
#define REMOVE_APP_REQ 0x14
#define REMOVE_APP_RES 0x15
#define FILE_UPLOAD 0x60
#define FILE_UPLOAD_DONE 0x61
#define FILE_DOWNLOAD 0x62
#define FILE_DOWNLOAD_DATA 0x63
#define PROTOCOL_VERSION_REQ 0x7C
#define PROTOCOL_VERSION_RES 0x7D
#define ANNOUNCE_REVERSE_COMPATIBILITY 0x7F

#define LATEST_SUPPORTED_VERSION 1

#include "rxhandlers.hpp"
#include "txhandlers.hpp"
struct rxTransmission
{
    packetRxHandler handler;
    void *ctx;
    uint32_t following;
};

struct txTransmission
{
    packetTxHandler handler;
    void *ctx;
    uint32_t following;
    uint32_t successful;
};

void uart_init();

// This function will return true when the write was a success, and false otherwise.
bool uart_protocol_write_base_v1(uint8_t id, uint32_t following, uint8_t *body, uint16_t bodylen);
bool uart_protocol_write_followup_v1(uint8_t id, uint8_t *body, uint16_t bodylen);