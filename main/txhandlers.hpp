#ifndef __TXHANDLERS_H__
#define __TXHANDLERS_H__

#include <inttypes.h>
#include <stdio.h>

struct txShortBodyContext
{
    uint16_t size;
    uint8_t *data;
};

#define TX_SHORT_BODY_HANDLER                                    \
    (transmissionTxHandler)                                      \
    {                                                            \
        .getFirstBuffer = get_first_buffer_simple_body,          \
        .getFollowupBuffer = get_followup_no_action,             \
        .transmissionFinished = transmission_finished_no_action, \
    }

#define TX_NO_BODY_HANDLER                                       \
    (transmissionTxHandler)                                      \
    {                                                            \
        .getFirstBuffer = get_first_buffer_no_action,            \
        .getFollowupBuffer = get_followup_no_action,             \
        .transmissionFinished = transmission_finished_no_action, \
    }
struct transmissionTxHandler
{
    // These 2 first functions write in the buf and return the body size written.
    uint16_t (*getFirstBuffer)(void *ctx, uint8_t *buf);
    // followupn indicates the number of followup packets sent successfully before him.
    uint16_t (*getFollowupBuffer)(void *ctx, uint8_t *buf, u_int32_t followupn);

    void (*transmissionFinished)(void *ctx);
};

uint16_t get_first_buffer_simple_body(void *ctx, uint8_t *buf);
uint16_t get_first_buffer_no_action(void *ctx, uint8_t *buf);
uint16_t get_followup_no_action(void *ctx, uint8_t *buf, u_int32_t followupn);
void transmission_finished_no_action(void *ctx);

#endif // __TXHANDLERS_H__