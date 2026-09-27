#ifndef FORGE_MESSAGE_H
#define FORGE_MESSAGE_H

#include <stdint.h>

typedef enum {
    FORGE_MESSAGE_PING = 1,
    FORGE_MESSAGE_PONG = 2,
    FORGE_MESSAGE_CONNECT = 3,
    FORGE_MESSAGE_DISCONNECT = 4,
    FORGE_MESSAGE_ERROR = 5
} ForgeMessageType;

typedef struct {
    uint8_t type;
    uint16_t flags;
    uint32_t request_id;

    const uint8_t *payload;
    uint32_t payload_length;
} ForgeMessage;

#endif