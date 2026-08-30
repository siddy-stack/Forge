#include "server.h"
#include "network.h"
#include "connection.h"
#include "connection_manager.h"
#include "event_loop.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

int forge_server_run(uint16_t port)
{
    int server_fd = forge_network_create_listener(port);

    if (server_fd == -1) {
        return 1;
    }

    if (forge_network_set_nonblocking(server_fd) == -1) {
        close(server_fd);
        return 1;
    }

    ForgeEventLoop event_loop;

    if (forge_event_loop_init(&event_loop) == -1) {
        close(server_fd);
        return 1;
    }

    if (forge_event_loop_add(
            &event_loop,
            server_fd,
            EPOLLIN
        ) == -1) {
        forge_event_loop_destroy(&event_loop);
        close(server_fd);
        return 1;
    }

    ForgeConnectionManager connection_manager;

    forge_connection_manager_init(&connection_manager);

    printf("Forge server listening on port %u\n", port);

    for (;;) {
        int event_count = forge_event_loop_wait(
            &event_loop,
            -1
        );

        if (event_count == -1) {
            break;
        }

        for (int i = 0; i < event_count; i++) {
            int fd = forge_event_loop_get_fd(
                &event_loop,
                i
            );

            uint32_t events = forge_event_loop_get_events(
                &event_loop,
                i
            );

            if (events & (EPOLLERR | EPOLLHUP)) {
                if (fd != server_fd) {
                    forge_event_loop_remove(
                        &event_loop,
                        fd
                    );

                    forge_connection_manager_remove(
                        &connection_manager,
                        fd
                    );
                }

                continue;
            }

            if (fd == server_fd) {
                for (;;) {
                    int client_fd =
                        forge_network_accept_client(
                            server_fd
                        );

                    if (client_fd == -1) {
                        break;
                    }

                    if (forge_network_set_nonblocking(
                            client_fd
                        ) == -1) {
                        close(client_fd);
                        continue;
                    }

                    ForgeConnection *connection =
                        malloc(sizeof(*connection));

                    if (connection == NULL) {
                        fprintf(
                            stderr,
                            "Failed to allocate connection.\n"
                        );

                        close(client_fd);
                        continue;
                    }

                    forge_connection_init(
                        connection,
                        client_fd
                    );

                    if (forge_connection_manager_add(
                            &connection_manager,
                            connection
                        ) == -1) {
                        forge_connection_close(connection);
                        free(connection);
                        continue;
                    }

                    if (forge_event_loop_add(
                            &event_loop,
                            client_fd,
                            EPOLLIN
                        ) == -1) {
                        forge_connection_manager_remove(
                            &connection_manager,
                            client_fd
                        );
                        continue;
                    }

                    printf(
                        "Client connected: fd=%d\n",
                        client_fd
                    );
                }

                continue;
            }

            ForgeConnection *connection =
                forge_connection_manager_get(
                    &connection_manager,
                    fd
                );

            if (connection == NULL) {
                fprintf(
                    stderr,
                    "Connection not found: fd=%d\n",
                    fd
                );
                continue;
            }

            int result =
                forge_connection_receive(connection);

            if (result == 0) {
                forge_event_loop_remove(
                    &event_loop,
                    fd
                );

                forge_connection_manager_remove(
                    &connection_manager,
                    fd
                );

                printf(
                    "Client disconnected: fd=%d\n",
                    fd
                );

                continue;
            }

            if (result == -1) {
                forge_event_loop_remove(
                    &event_loop,
                    fd
                );

                forge_connection_manager_remove(
                    &connection_manager,
                    fd
                );

                continue;
            }

            printf(
                "Received from fd=%d: %s",
                fd,
                connection->buffer
            );

            if (forge_connection_send(
                    connection,
                    connection->buffer,
                    connection->bytes_received
                ) == -1) {
                forge_event_loop_remove(
                    &event_loop,
                    fd
                );

                forge_connection_manager_remove(
                    &connection_manager,
                    fd
                );
            }
        }
    }

    forge_connection_manager_destroy(
        &connection_manager
    );

    forge_event_loop_destroy(
        &event_loop
    );

    close(server_fd);

    return 0;
}