#include "protocol.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t encode_message(
    const ForgeMessage *message,
    uint8_t *buffer,
    size_t buffer_size
)
{
    int result = forge_protocol_encode(
        message,
        buffer,
        buffer_size
    );

    assert(result > 0);

    return (size_t)result;
}

static void test_encode_decode(void)
{
    const uint8_t payload[] = "hello forge";

    ForgeMessage original = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0x1234,
        .request_id = 42,
        .payload = payload,
        .payload_length = sizeof(payload) - 1,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    ];

    size_t encoded_size = encode_message(
        &original,
        buffer,
        sizeof(buffer)
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size,
        &decoded,
        &bytes_consumed
    );

    assert(result == 1);
    assert(bytes_consumed == encoded_size);

    assert(decoded.type == original.type);
    assert(decoded.flags == original.flags);
    assert(decoded.request_id == original.request_id);

    assert(
        decoded.payload_length ==
        original.payload_length
    );

    assert(memcmp(
        decoded.payload,
        original.payload,
        original.payload_length
    ) == 0);
}

static void test_incomplete_message(void)
{
    const uint8_t payload[] = "hello";

    ForgeMessage message = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0,
        .request_id = 1,
        .payload = payload,
        .payload_length = sizeof(payload) - 1,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    ];

    size_t encoded_size = encode_message(
        &message,
        buffer,
        sizeof(buffer)
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size - 1,
        &decoded,
        &bytes_consumed
    );

    assert(result == 0);
}

static void test_invalid_magic(void)
{
    const uint8_t payload[] = "test";

    ForgeMessage message = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0,
        .request_id = 100,
        .payload = payload,
        .payload_length = sizeof(payload) - 1,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    ];

    size_t encoded_size = encode_message(
        &message,
        buffer,
        sizeof(buffer)
    );

    buffer[0] ^= 0xFF;

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size,
        &decoded,
        &bytes_consumed
    );

    assert(result == -1);
}

static void test_empty_payload(void)
{
    ForgeMessage message = {
        .type = FORGE_MESSAGE_PONG,
        .flags = 0,
        .request_id = 200,
        .payload = NULL,
        .payload_length = 0,
    };

    uint8_t buffer[FORGE_PROTOCOL_HEADER_SIZE];

    size_t encoded_size = encode_message(
        &message,
        buffer,
        sizeof(buffer)
    );

    assert(
        encoded_size ==
        FORGE_PROTOCOL_HEADER_SIZE
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size,
        &decoded,
        &bytes_consumed
    );

    assert(result == 1);
    assert(decoded.type == FORGE_MESSAGE_PONG);
    assert(decoded.request_id == 200);
    assert(decoded.payload_length == 0);
    assert(decoded.payload == NULL);
}

static void test_maximum_payload(void)
{
    uint8_t payload[FORGE_PROTOCOL_MAX_PAYLOAD];

    memset(
        payload,
        0xAB,
        sizeof(payload)
    );

    ForgeMessage message = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0,
        .request_id = 300,
        .payload = payload,
        .payload_length = FORGE_PROTOCOL_MAX_PAYLOAD,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    ];

    size_t encoded_size = encode_message(
        &message,
        buffer,
        sizeof(buffer)
    );

    assert(
        encoded_size ==
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size,
        &decoded,
        &bytes_consumed
    );

    assert(result == 1);

    assert(
        decoded.payload_length ==
        FORGE_PROTOCOL_MAX_PAYLOAD
    );

    assert(
        memcmp(
            decoded.payload,
            payload,
            FORGE_PROTOCOL_MAX_PAYLOAD
        ) == 0
    );
}

static void test_payload_too_large(void)
{
    uint8_t payload[
        FORGE_PROTOCOL_MAX_PAYLOAD + 1
    ];

    memset(
        payload,
        0xCD,
        sizeof(payload)
    );

    ForgeMessage message = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0,
        .request_id = 400,
        .payload = payload,
        .payload_length =
            FORGE_PROTOCOL_MAX_PAYLOAD + 1,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD + 1
    ];

    int result = forge_protocol_encode(
        &message,
        buffer,
        sizeof(buffer)
    );

    assert(result == -1);
}

static void test_malformed_payload_length(void)
{
    const uint8_t payload[] = "test";

    ForgeMessage message = {
        .type = FORGE_MESSAGE_PING,
        .flags = 0,
        .request_id = 500,
        .payload = payload,
        .payload_length = sizeof(payload) - 1,
    };

    uint8_t buffer[
        FORGE_PROTOCOL_HEADER_SIZE +
        FORGE_PROTOCOL_MAX_PAYLOAD
    ];

    size_t encoded_size = encode_message(
        &message,
        buffer,
        sizeof(buffer)
    );

    /*
     * Set the payload length to one byte beyond
     * the maximum allowed value.
     *
     * Payload length occupies bytes 6-9.
     */
    uint32_t invalid_length =
        htonl(
            FORGE_PROTOCOL_MAX_PAYLOAD + 1
        );

    memcpy(
        buffer + 6,
        &invalid_length,
        sizeof(invalid_length)
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        encoded_size,
        &decoded,
        &bytes_consumed
    );

    assert(result == -1);
}

static void test_incomplete_header(void)
{
    uint8_t buffer[FORGE_PROTOCOL_HEADER_SIZE - 1];

    memset(
        buffer,
        0,
        sizeof(buffer)
    );

    ForgeMessage decoded;
    size_t bytes_consumed = 0;

    int result = forge_protocol_decode(
        buffer,
        sizeof(buffer),
        &decoded,
        &bytes_consumed
    );

    assert(result == 0);
}

int main(void)
{
    test_encode_decode();
    test_incomplete_message();
    test_invalid_magic();
    test_empty_payload();

    test_maximum_payload();
    test_payload_too_large();
    test_malformed_payload_length();
    test_incomplete_header();

    printf("All protocol tests passed.\n");

    return 0;
}