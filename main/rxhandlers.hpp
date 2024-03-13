#ifndef __RXHANDLERS_H__
#define __RXHANDLERS_H__

#include <inttypes.h>
#include <stdio.h>

void *handler_no_action(uint8_t *buf, size_t buflen);
void handler_following_no_action(void *ctx, uint8_t *buf, size_t buflen);

#define RX_NO_BODY_HANDLER(transmissionFinishedHandler)        \
    (transmissionRxHandler)                                    \
    {                                                          \
        .firstBufferHandler = handler_no_action,               \
        .followingBufferHandler = handler_following_no_action, \
        .transmissionFinished = transmissionFinishedHandler,   \
    }

struct transmissionRxHandler
{
    // If data received in buffers is not satisfying, just ignore it.
    // This one returns a pointer that must me forwarded to the other functions
    void *(*firstBufferHandler)(uint8_t *buf, size_t buflen);

    // If data received in buffers is not satisfying, just ignore it.
    void (*followingBufferHandler)(void *ctx, uint8_t *buf, size_t buflen);

    // Response packets must only be sent when this function is called.
    // When invalidateData is set to true, all buffer content already sent before must be reset.
    // So data received should not have consequencies until this function is called.
    void (*transmissionFinished)(void *ctx, bool invalidateData);
};

transmissionRxHandler uart_get_handler_from_msgid(uint8_t id);
#endif // __RXHANDLERS_H__