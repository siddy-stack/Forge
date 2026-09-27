#ifndef FORGE_PROTOCOL_H
#define FORGE_PROTOCOL_H

#include "message.h"

#include <stddef.h>
#include <stdint.h>

#define FORGE_PROTOCOL_MAGIC 0x4647
#define FORGE_PROTOCOL_VERSION 1

#define FORGE_PROTOCOL_HEADER_SIZE 16
#define FORGE_PROTOCOL_MAX_PAYLOAD 65536

int forge_protocol_encode(
    const ForgeMessage *message,
    uint8_t *buffer,
    size_t buffer_size
);

int forge_protocol_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    ForgeMessage *message,
    size_t *bytes_consumed
);

#endif