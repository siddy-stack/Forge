#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static uint64_t forge_load_now_us(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) == -1)
    {
        return 0;
    }

    return (uint64_t)value.tv_sec * UINT64_C(1000000) +
           (uint64_t)value.tv_nsec / UINT64_C(1000);
}

static int forge_load_compare_u64(const void *left, const void *right)
{
    uint64_t left_value = *(const uint64_t *)left;
    uint64_t right_value = *(const uint64_t *)right;

    return (left_value > right_value) - (left_value < right_value);
}

static int forge_load_connect(void)
{
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {.sin_family = AF_INET,
                                  .sin_port = htons(8080),
                                  .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};

    if (file_descriptor < 0 ||
        connect(file_descriptor, (struct sockaddr *)&address,
                sizeof(address)) == -1)
    {
        if (file_descriptor >= 0)
        {
            close(file_descriptor);
        }
        return -1;
    }

    return file_descriptor;
}

static int forge_load_send_all(int file_descriptor, const uint8_t *buffer,
                               size_t length)
{
    size_t offset = 0;

    while (offset < length)
    {
        ssize_t sent =
            send(file_descriptor, buffer + offset, length - offset, 0);

        if (sent <= 0)
        {
            if (sent == -1 && errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        offset += (size_t)sent;
    }
    return 0;
}

static int forge_load_receive_pong(int file_descriptor)
{
    uint8_t buffer[FORGE_PROTOCOL_HEADER_SIZE];
    size_t size = 0;

    while (size < sizeof(buffer))
    {
        ForgeMessage message;
        size_t consumed = 0;
        ssize_t received =
            recv(file_descriptor, buffer + size, sizeof(buffer) - size, 0);

        if (received <= 0)
        {
            if (received == -1 && errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        size += (size_t)received;
        if (forge_protocol_decode(buffer, size, &message, &consumed) == 1)
        {
            return message.type == FORGE_MESSAGE_PONG &&
                           message.payload_length == 0U
                       ? 0
                       : -1;
        }
    }
    return -1;
}

int main(int argument_count, char **arguments)
{
    char *end;
    long requested_connections;
    long messages_per_connection;
    size_t connected = 0;
    size_t completed = 0;
    int *connections;
    uint64_t *latencies;
    uint64_t started_at;
    uint64_t elapsed_us;

    if (argument_count != 3)
    {
        fprintf(stderr, "Usage: %s CONNECTIONS MESSAGES_PER_CONNECTION\n",
                arguments[0]);
        return 2;
    }
    errno = 0;
    requested_connections = strtol(arguments[1], &end, 10);
    if (errno != 0 || *end != '\0' || requested_connections < 1 ||
        requested_connections > 10000)
    {
        fprintf(stderr, "CONNECTIONS must be between 1 and 10000.\n");
        return 2;
    }
    errno = 0;
    messages_per_connection = strtol(arguments[2], &end, 10);
    if (errno != 0 || *end != '\0' || messages_per_connection < 1 ||
        messages_per_connection > 1000 ||
        requested_connections * messages_per_connection > 1000000)
    {
        fprintf(stderr, "Workload must contain at most 1000000 messages.\n");
        return 2;
    }

    connections = calloc((size_t)requested_connections, sizeof(*connections));
    latencies =
        calloc((size_t)requested_connections * (size_t)messages_per_connection,
               sizeof(*latencies));
    if (connections == NULL || latencies == NULL)
    {
        free(connections);
        free(latencies);
        return 1;
    }
    for (long index = 0; index < requested_connections; index++)
    {
        int file_descriptor = forge_load_connect();
        if (file_descriptor < 0)
        {
            break;
        }
        connections[connected++] = file_descriptor;
    }

    started_at = forge_load_now_us();
    for (size_t index = 0; index < connected; index++)
    {
        for (long message_index = 0; message_index < messages_per_connection;
             message_index++)
        {
            uint8_t buffer[FORGE_PROTOCOL_HEADER_SIZE];
            ForgeMessage message = {.type = FORGE_MESSAGE_PING,
                                    .flags = 0,
                                    .request_id = (uint32_t)completed,
                                    .payload = NULL,
                                    .payload_length = 0};
            uint64_t sent_at;
            int encoded =
                forge_protocol_encode(&message, buffer, sizeof(buffer));

            if (encoded < 0)
            {
                break;
            }
            sent_at = forge_load_now_us();
            if (forge_load_send_all(connections[index], buffer,
                                    (size_t)encoded) == -1 ||
                forge_load_receive_pong(connections[index]) == -1)
            {
                break;
            }
            latencies[completed++] = forge_load_now_us() - sent_at;
        }
    }
    elapsed_us = forge_load_now_us() - started_at;
    for (size_t index = 0; index < connected; index++)
    {
        close(connections[index]);
    }
    if (completed > 0U)
    {
        qsort(latencies, completed, sizeof(*latencies), forge_load_compare_u64);
        printf("connected=%zu messages=%zu messages_per_second=%.2f "
               "latency_us_p50=%" PRIu64 " latency_us_p95=%" PRIu64
               " latency_us_p99=%" PRIu64 "\n",
               connected, completed,
               elapsed_us == 0U
                   ? 0.0
                   : (double)completed * 1000000.0 / (double)elapsed_us,
               latencies[(completed - 1U) * 50U / 100U],
               latencies[(completed - 1U) * 95U / 100U],
               latencies[(completed - 1U) * 99U / 100U]);
    }
    free(connections);
    free(latencies);
    return completed == connected * (size_t)messages_per_connection ? 0 : 1;
}
