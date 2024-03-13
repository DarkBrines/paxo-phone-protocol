#ifndef __TXHANDLERS_H__
#define __TXHANDLERS_H__

#include <inttypes.h>
#include <stdio.h>

struct packetTxHandler
{
    // These 2 first functions write in the buf and return the body size written.
    uint16_t (*getFirstBuffer)(void *ctx, uint8_t *buf);
    // followupn indicates the number of followup packets sent successfully before him.
    uint16_t (*getFollowupBuffer)(void *ctx, uint8_t *buf, u_int32_t followupn);

    void (*transmissionFinished)(void *ctx);
};
#endif // __TXHANDLERS_H__