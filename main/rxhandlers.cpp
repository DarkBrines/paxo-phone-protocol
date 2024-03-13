#include "rxhandlers.hpp"
#include "protocol.hpp"
#include <map>

void protocol_version_request(void *ctx, bool invalidateData)
{
    if (invalidateData)
        return;

    uint8_t pl = LATEST_SUPPORTED_VERSION;
    txShortBodyContext sbctx = {
        .size = 1,
        .data = &pl,
    };

    uart_protocol_start_transmission(
        PROTOCOL_VERSION_RES, TX_SHORT_BODY_HANDLER, &sbctx, 0);
}

void *handler_no_action(uint8_t *buf, size_t buflen)
{
    return nullptr;
}

void handler_following_no_action(void *ctx, uint8_t *buf, size_t buflen)
{
    return;
}

void transmission_finished_no_action(void *ctx, bool invalidateData)
{
    return;
}

static std::map<uint8_t, transmissionRxHandler> handlers = {
    {PROTOCOL_VERSION_REQ, RX_NO_BODY_HANDLER(protocol_version_request)},
};

transmissionRxHandler uart_get_handler_from_msgid(uint8_t id)
{
    if (handlers.find(id) == handlers.end())
    {
        return RX_NO_BODY_HANDLER(nullptr);
    }
    return handlers[id];
}