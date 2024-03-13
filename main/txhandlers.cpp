#include "txhandlers.hpp"
#include "protocol.hpp"
#include <map>
#include <cstring>

uint16_t get_first_buffer_simple_body(void *ctx, uint8_t *buf)
{
    txShortBodyContext *sbctx = (txShortBodyContext *)ctx;
    memcpy(buf, sbctx->data, sbctx->size);
    return sbctx->size;
}

uint16_t get_first_buffer_no_action(void *ctx, uint8_t *buf)
{
    return 0;
}

uint16_t get_followup_no_action(void *ctx, uint8_t *buf, u_int32_t followupn)
{
    return 0;
}

void transmission_finished_no_action(void *ctx)
{
    return;
}