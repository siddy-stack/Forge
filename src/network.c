#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int forge_network_create_listener(uint16_t port)
{
    int server_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (server_file_descriptor == -1)
    {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));

        return -1;
    }

    int reuse_address = 1;

    if (setsockopt(server_file_descriptor, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) == -1)
    {
        fprintf(stderr, "Failed to configure socket: %s\n", strerror(errno));

        close(server_file_descriptor);

        return -1;
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port),
    };

    if (bind(server_file_descriptor, (struct sockaddr *)&address,
             sizeof(address)) == -1)
    {
        fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));

        close(server_file_descriptor);

        return -1;
    }

    if (listen(server_file_descriptor, FORGE_BACKLOG) == -1)
    {
        fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));

        close(server_file_descriptor);

        return -1;
    }

    return server_file_descriptor;
}

int forge_network_accept_client(int server_file_descriptor)
{
    struct sockaddr_in client_address;

    socklen_t client_length = sizeof(client_address);

    int client_file_descriptor =
        accept(server_file_descriptor, (struct sockaddr *)&client_address,
               &client_length);

    if (client_file_descriptor == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -1;
        }

        fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));

        return -1;
    }

    return client_file_descriptor;
}

int forge_network_set_nonblocking(int file_descriptor)
{
    int flags = fcntl(file_descriptor, F_GETFL, 0);

    if (flags == -1)
    {
        fprintf(stderr, "Failed to get socket flags: %s\n", strerror(errno));

        return -1;
    }

    if (fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        fprintf(stderr, "Failed to set socket non-blocking: %s\n",
                strerror(errno));

        return -1;
    }

    return 0;
}
