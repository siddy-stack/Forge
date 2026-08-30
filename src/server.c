#include "server.h"
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int forge_server_run(uint16_t port)
{
    int server_fd = forge_network_create_listener(port);

    if (server_fd == -1) {
        return 1;
    }

    printf("Forge server listening on port %u\n", port);

    for (;;) {
        int client_fd = forge_network_accept_client(server_fd);

        if (client_fd == -1) {
            continue;
        }

        printf("Client connected.\n");

        char buffer[1024];

        for (;;) {
            ssize_t bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer) - 1,
                0
            );

            if (bytes_received == 0) {
                printf("Client disconnected.\n");
                break;
            }

            if (bytes_received == -1) {
                fprintf(
                    stderr,
                    "Failed to receive data.\n"
                );
                break;
            }

            buffer[bytes_received] = '\0';

            printf("Received: %s", buffer);

            ssize_t bytes_sent = send(
                client_fd,
                buffer,
                (size_t)bytes_received,
                0
            );

            if (bytes_sent == -1) {
                fprintf(
                    stderr,
                    "Failed to send response.\n"
                );
                break;
            }
        }

        close(client_fd);
    }

    close(server_fd);

    return 0;
}