#include "server.h"
#include "network.h"
#include "connection.h"

#include <stdio.h>
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

        ForgeConnection connection;

        forge_connection_init(&connection, client_fd);

        for (;;) {
            int result = forge_connection_receive(&connection);

            if (result == 0) {
                printf("Client disconnected.\n");
                break;
            }

            if (result == -1) {
                break;
            }

            printf("Received: %s", connection.buffer);

            if (forge_connection_send(
                    &connection,
                    connection.buffer,
                    connection.bytes_received
                ) == -1) {
                break;
            }
        }

        forge_connection_close(&connection);
    }

    close(server_fd);

    return 0;
}