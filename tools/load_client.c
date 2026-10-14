#define _POSIX_C_SOURCE 200809L

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

static uint64_t now_ms(void)
{
    struct timespec time_value;
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) == -1)
    {
        return 0;
    }
    return (uint64_t)time_value.tv_sec * 1000U +
           (uint64_t)time_value.tv_nsec / 1000000U;
}

static int connect_client(uint16_t port)
{
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;

    if (file_descriptor < 0)
    {
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        connect(file_descriptor, (struct sockaddr *)&address,
                sizeof(address)) == -1)
    {
        close(file_descriptor);
        return -1;
    }
    return file_descriptor;
}

int main(int argument_count, char **arguments)
{
    long requested_connections;
    long hold_seconds;
    char *end;
    int *connections;
    size_t connected = 0;
    uint64_t started_at;
    uint64_t elapsed_ms;

    if (argument_count != 3)
    {
        fprintf(stderr, "Usage: %s CONNECTIONS HOLD_SECONDS\n", arguments[0]);
        return 2;
    }
    errno = 0;
    requested_connections = strtol(arguments[1], &end, 10);
    if (errno != 0 || *end != '\0' || requested_connections <= 0 ||
        requested_connections > 100000)
    {
        fprintf(stderr, "CONNECTIONS must be between 1 and 100000.\n");
        return 2;
    }
    errno = 0;
    hold_seconds = strtol(arguments[2], &end, 10);
    if (errno != 0 || *end != '\0' || hold_seconds < 0 || hold_seconds > 3600)
    {
        fprintf(stderr, "HOLD_SECONDS must be between 0 and 3600.\n");
        return 2;
    }
    connections = calloc((size_t)requested_connections, sizeof(*connections));
    if (connections == NULL)
    {
        return 1;
    }
    started_at = now_ms();
    for (long index = 0; index < requested_connections; index++)
    {
        int file_descriptor = connect_client(8080);
        if (file_descriptor < 0)
        {
            break;
        }
        connections[connected++] = file_descriptor;
    }
    elapsed_ms = now_ms() - started_at;
    printf("requested=%ld connected=%zu rejected=%ld elapsed_ms=%" PRIu64
           " connections_per_second=%.2f\n",
           requested_connections, connected,
           requested_connections - (long)connected, elapsed_ms,
           elapsed_ms == 0U ? 0.0
                            : (double)connected * 1000.0 / (double)elapsed_ms);
    if (hold_seconds > 0)
    {
        struct timespec delay = {.tv_sec = hold_seconds, .tv_nsec = 0};
        (void)nanosleep(&delay, NULL);
    }
    for (size_t index = 0; index < connected; index++)
    {
        close(connections[index]);
    }
    free(connections);
    return connected == (size_t)requested_connections ? 0 : 1;
}
