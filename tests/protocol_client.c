#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum
{
    FORGE_TEST_PORT = 8080,

    FORGE_TEST_BUFFER_SIZE =
        FORGE_PROTOCOL_HEADER_SIZE + FORGE_PROTOCOL_MAX_PAYLOAD
};

static int connect_to_server(void)
{
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (file_descriptor == -1)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_address;

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(FORGE_TEST_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) != 1)
    {
        perror("inet_pton");
        close(file_descriptor);
        return -1;
    }

    if (connect(file_descriptor, (struct sockaddr *)&server_address,
                sizeof(server_address)) == -1)
    {
        perror("connect");
        close(file_descriptor);
        return -1;
    }

    return file_descriptor;
}

static int send_all(int file_descriptor, const uint8_t *buffer, size_t length)
{
    size_t offset = 0;

    while (offset < length)
    {
        ssize_t sent =
            send(file_descriptor, buffer + offset, length - offset, 0);

        if (sent == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("send");
            return -1;
        }

        if (sent == 0)
        {
            return -1;
        }

        offset += (size_t)sent;
    }

    return 0;
}

static int send_fragmented(int file_descriptor, const uint8_t *buffer,
                           size_t length)
{
    size_t first_chunk = 3;

    if (length < first_chunk)
    {
        return -1;
    }

    printf("Sending fragmented message: "
           "%zu + %zu bytes\n",
           first_chunk, length - first_chunk);

    if (send_all(file_descriptor, buffer, first_chunk) == -1)
    {
        return -1;
    }

    struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000};

    nanosleep(&delay, NULL);

    if (send_all(file_descriptor, buffer + first_chunk, length - first_chunk) ==
        -1)
    {
        return -1;
    }

    return 0;
}

static int receive_messages(int file_descriptor, ForgeMessage *messages,
                            uint8_t payloads[][FORGE_PROTOCOL_MAX_PAYLOAD],
                            size_t message_count)
{
    uint8_t buffer[FORGE_TEST_BUFFER_SIZE * 2];

    size_t buffer_size = 0;
    size_t received_messages = 0;

    while (received_messages < message_count)
    {
        if (buffer_size == sizeof(buffer))
        {
            fprintf(stderr, "Receive buffer is full.\n");

            return -1;
        }

        ssize_t received = recv(file_descriptor, buffer + buffer_size,
                                sizeof(buffer) - buffer_size, 0);

        if (received == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("recv");
            return -1;
        }

        if (received == 0)
        {
            fprintf(stderr, "Server closed connection.\n");

            return -1;
        }

        buffer_size += (size_t)received;

        for (;;)
        {
            if (received_messages >= message_count)
            {
                break;
            }

            size_t bytes_consumed = 0;
            ForgeMessage decoded;

            int result = forge_protocol_decode(buffer, buffer_size, &decoded,
                                               &bytes_consumed);

            if (result == 0)
            {
                break;
            }

            if (result == -1)
            {
                fprintf(stderr, "Received invalid Forge message.\n");

                return -1;
            }

            if (decoded.payload_length > FORGE_PROTOCOL_MAX_PAYLOAD)
            {
                return -1;
            }

            if (decoded.payload_length > 0)
            {
                memcpy(payloads[received_messages], decoded.payload,
                       decoded.payload_length);
            }

            messages[received_messages] = decoded;

            messages[received_messages].payload = payloads[received_messages];

            printf("Received message: "
                   "type=%u request_id=%u "
                   "payload=%u bytes\n",
                   messages[received_messages].type,
                   messages[received_messages].request_id,
                   messages[received_messages].payload_length);

            size_t remaining = buffer_size - bytes_consumed;

            memmove(buffer, buffer + bytes_consumed, remaining);

            buffer_size = remaining;
            received_messages++;
        }
    }

    return 0;
}

static void test_fragmented_message(int file_descriptor)
{
    const uint8_t payload[] = "hello from fragmented TCP";

    ForgeMessage message = {.type = FORGE_MESSAGE_PING,
                            .flags = 0x1234,
                            .request_id = 1001,
                            .payload = payload,
                            .payload_length = sizeof(payload) - 1};

    uint8_t buffer[FORGE_TEST_BUFFER_SIZE];

    int encoded_size = forge_protocol_encode(&message, buffer, sizeof(buffer));

    assert(encoded_size > 0);

    printf("\nTest 1: fragmented message\n");

    assert(send_fragmented(file_descriptor, buffer, (size_t)encoded_size) == 0);

    ForgeMessage response;

    uint8_t response_payload[FORGE_PROTOCOL_MAX_PAYLOAD];

    assert(receive_messages(file_descriptor, &response, &response_payload, 1) ==
           0);

    assert(response.type == FORGE_MESSAGE_PONG);

    assert(response.flags == message.flags);

    assert(response.request_id == message.request_id);

    assert(response.payload_length == message.payload_length);

    assert(memcmp(response.payload, message.payload, message.payload_length) ==
           0);

    printf("Fragmented message test passed.\n");
}

static void test_multiple_messages(int file_descriptor)
{
    const uint8_t payload1[] = "first message";

    const uint8_t payload2[] = "second message";

    ForgeMessage message1 = {.type = FORGE_MESSAGE_PING,
                             .flags = 0,
                             .request_id = 2001,
                             .payload = payload1,
                             .payload_length = sizeof(payload1) - 1};

    ForgeMessage message2 = {.type = FORGE_MESSAGE_PING,
                             .flags = 0,
                             .request_id = 2002,
                             .payload = payload2,
                             .payload_length = sizeof(payload2) - 1};

    uint8_t buffer[FORGE_TEST_BUFFER_SIZE * 2];

    int size1 = forge_protocol_encode(&message1, buffer, sizeof(buffer));

    assert(size1 > 0);

    int size2 = forge_protocol_encode(&message2, buffer + size1,
                                      sizeof(buffer) - (size_t)size1);

    assert(size2 > 0);

    size_t total_size = (size_t)size1 + (size_t)size2;

    printf("\nTest 2: multiple messages "
           "in one TCP stream\n");

    assert(send_all(file_descriptor, buffer, total_size) == 0);

    ForgeMessage responses[2];

    uint8_t response_payloads[2][FORGE_PROTOCOL_MAX_PAYLOAD];

    assert(receive_messages(file_descriptor, responses, response_payloads, 2) ==
           0);

    assert(responses[0].type == FORGE_MESSAGE_PONG);

    assert(responses[0].request_id == message1.request_id);

    assert(responses[0].payload_length == message1.payload_length);

    assert(memcmp(responses[0].payload, message1.payload,
                  message1.payload_length) == 0);

    assert(responses[1].type == FORGE_MESSAGE_PONG);

    assert(responses[1].request_id == message2.request_id);

    assert(responses[1].payload_length == message2.payload_length);

    assert(memcmp(responses[1].payload, message2.payload,
                  message2.payload_length) == 0);

    printf("Multiple message test passed.\n");
}

int main(void)
{
    printf("Connecting to Forge on "
           "127.0.0.1:%d...\n",
           FORGE_TEST_PORT);

    int file_descriptor = connect_to_server();

    if (file_descriptor == -1)
    {
        return 1;
    }

    printf("Connected.\n");

    test_fragmented_message(file_descriptor);

    test_multiple_messages(file_descriptor);

    close(file_descriptor);

    printf("\nAll Forge protocol integration "
           "tests passed.\n");

    return 0;
}
