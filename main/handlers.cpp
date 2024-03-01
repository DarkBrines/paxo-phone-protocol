#include "handlers.hpp"
#include "protocol.hpp"
#include <map>

void protocol_version_request(void *ctx, bool invalidateData)
{
    if (invalidateData)
        return;

    uint8_t pl = LATEST_SUPPORTED_VERSION;
    uart_protocol_write_base_v1(PROTOCOL_VERSION_RES, 0, &pl, 1);
}

void *handler_no_action(uint8_t *buf, size_t buflen)
{
    return nullptr;
}

void handler_following_no_action(void *ctx, uint8_t *buf, size_t buflen)
{
    return;
}

void packet_finished_no_action(void *ctx, bool invalidateData)
{
    return;
}

static std::map<uint8_t, packetHandler> handlers = {
    {PROTOCOL_VERSION_REQ, NO_BODY_HANDLER(protocol_version_request)},
};

packetHandler uart_get_handler_from_id(uint8_t id)
{
    if (handlers.find(id) == handlers.end())
    {
        return NO_BODY_HANDLER(nullptr);
    }
    return handlers[id];
}