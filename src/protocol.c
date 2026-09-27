#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

int forge_protocol_encode(
    const ForgeMessage *message,
    uint8_t *buffer,
    size_t buffer_size
)
{
    if (message == NULL || buffer == NULL) {
        return -1;
    }

    if (message->payload_length >
        FORGE_PROTOCOL_MAX_PAYLOAD) {
        return -1;
    }

    size_t total_size =
        FORGE_PROTOCOL_HEADER_SIZE +
        message->payload_length;

    if (buffer_size < total_size) {
        return -1;
    }

    if (message->payload_length > 0 &&
        message->payload == NULL) {
        return -1;
    }

    uint16_t magic =
        htons(FORGE_PROTOCOL_MAGIC);

    uint16_t flags =
        htons(message->flags);

    uint32_t payload_length =
        htonl(message->payload_length);

    uint32_t request_id =
        htonl(message->request_id);

    memcpy(
        buffer,
        &magic,
        sizeof(magic)
    );

    buffer[2] = FORGE_PROTOCOL_VERSION;
    buffer[3] = message->type;

    memcpy(
        buffer + 4,
        &flags,
        sizeof(flags)
    );

    memcpy(
        buffer + 6,
        &payload_length,
        sizeof(payload_length)
    );

    memcpy(
        buffer + 10,
        &request_id,
        sizeof(request_id)
    );

    /*
     * Reserved bytes.
     */
    buffer[14] = 0;
    buffer[15] = 0;

    if (message->payload_length > 0) {
        memcpy(
            buffer + FORGE_PROTOCOL_HEADER_SIZE,
            message->payload,
            message->payload_length
        );
    }

    return (int)total_size;
}

int forge_protocol_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    ForgeMessage *message,
    size_t *bytes_consumed
)
{
    if (buffer == NULL ||
        message == NULL ||
        bytes_consumed == NULL) {
        return -1;
    }

    if (buffer_size < FORGE_PROTOCOL_HEADER_SIZE) {
        return 0;
    }

    uint16_t magic;

    memcpy(
        &magic,
        buffer,
        sizeof(magic)
    );

    magic = ntohs(magic);

    if (magic != FORGE_PROTOCOL_MAGIC) {
        return -1;
    }

    if (buffer[2] != FORGE_PROTOCOL_VERSION) {
        return -1;
    }

    uint16_t flags;

    memcpy(
        &flags,
        buffer + 4,
        sizeof(flags)
    );

    flags = ntohs(flags);

    uint32_t payload_length;

    memcpy(
        &payload_length,
        buffer + 6,
        sizeof(payload_length)
    );

    payload_length = ntohl(payload_length);

    if (payload_length >
        FORGE_PROTOCOL_MAX_PAYLOAD) {
        return -1;
    }

    uint32_t request_id;

    memcpy(
        &request_id,
        buffer + 10,
        sizeof(request_id)
    );

    request_id = ntohl(request_id);

    size_t total_size =
        FORGE_PROTOCOL_HEADER_SIZE +
        payload_length;

    /*
     * Header is valid, but the complete
     * payload has not arrived yet.
     */
    if (buffer_size < total_size) {
        return 0;
    }

    message->type = buffer[3];
    message->flags = flags;
    message->request_id = request_id;
    message->payload_length = payload_length;

    if (payload_length > 0) {
        message->payload =
            buffer + FORGE_PROTOCOL_HEADER_SIZE;
    } else {
        message->payload = NULL;
    }

    *bytes_consumed = total_size;

    return 1;
}