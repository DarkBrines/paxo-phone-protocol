#include <inttypes.h>
#include <stdio.h>

void *handler_no_action(uint8_t *buf, size_t buflen);
void handler_following_no_action(void *ctx, uint8_t *buf, size_t buflen);

#define NO_BODY_HANDLER(packetFinishedHandler)                 \
    (packetHandler)                                            \
    {                                                          \
        .firstBufferHandler = handler_no_action,               \
        .followingBufferHandler = handler_following_no_action, \
        .packetFinished = packetFinishedHandler,               \
    }

struct packetHandler
{
    // If data received in buffers is not satisfying, just ignore it.
    // This one returns a pointer that must me forwarded to the other functions
    void *(*firstBufferHandler)(uint8_t *buf, size_t buflen);

    // If data received in buffers is not satisfying, just ignore it.
    void (*followingBufferHandler)(void *ctx, uint8_t *buf, size_t buflen);

    // Response packets must only be sent when this function is called.
    // When invalidateData is set to true, all buffer content already sent before must be reset.
    // So data received should not have consequencies until this function is called.
    void (*packetFinished)(void *ctx, bool invalidateData);
};

packetHandler uart_get_handler_from_id(uint8_t id);