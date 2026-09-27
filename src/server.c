#include "server.h"

#include "client_handler.h"
#include "connection.h"
#include "connection_manager.h"
#include "event_loop.h"
#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

int forge_server_run(uint16_t port)
{
    int server_fd = forge_network_create_listener(port);

    if (server_fd == -1)
    {
        return 1;
    }

    if (forge_network_set_nonblocking(server_fd) == -1)
    {
        close(server_fd);
        return 1;
    }

    ForgeEventLoop event_loop;

    if (forge_event_loop_init(&event_loop) == -1)
    {
        close(server_fd);
        return 1;
    }

    if (forge_event_loop_add(&event_loop, server_fd, EPOLLIN) == -1)
    {
        forge_event_loop_destroy(&event_loop);
        close(server_fd);
        return 1;
    }

    ForgeConnectionManager connection_manager;

    forge_connection_manager_init(&connection_manager);

    printf("Forge server listening on port %u\n", port);

    for (;;)
    {
        int event_count = forge_event_loop_wait(&event_loop, -1);

        if (event_count == -1)
        {
            break;
        }

        for (int i = 0; i < event_count; i++)
        {
            ForgeEvent event;

            if (forge_event_loop_get_event(&event_loop, i, &event) == -1)
            {
                continue;
            }

            int fd = event.fd;
            uint32_t events = event.events;

            /*
             * Handle errors and closed connections.
             */
            if (events & (EPOLLERR | EPOLLHUP))
            {
                if (fd != server_fd)
                {
                    forge_event_loop_remove(&event_loop, fd);

                    forge_connection_manager_remove(&connection_manager, fd);
                }

                continue;
            }

            /*
             * The listening socket is ready.
             *
             * Accept every pending connection.
             */
            if (fd == server_fd)
            {
                for (;;)
                {
                    int client_fd = forge_network_accept_client(server_fd);

                    if (client_fd == -1)
                    {
                        break;
                    }

                    if (forge_network_set_nonblocking(client_fd) == -1)
                    {
                        close(client_fd);
                        continue;
                    }

                    ForgeConnection *connection = malloc(sizeof(*connection));

                    if (connection == NULL)
                    {
                        fprintf(stderr, "Failed to allocate connection.\n");

                        close(client_fd);
                        continue;
                    }

                    forge_connection_init(connection, client_fd);

                    if (forge_connection_manager_add(&connection_manager,
                                                     connection) == -1)
                    {
                        forge_connection_close(connection);

                        free(connection);
                        continue;
                    }

                    if (forge_event_loop_add(&event_loop, client_fd, EPOLLIN) ==
                        -1)
                    {
                        forge_connection_manager_remove(&connection_manager,
                                                        client_fd);

                        continue;
                    }

                    printf("Client connected: fd=%d\n", client_fd);
                }

                continue;
            }

            /*
             * Find the persistent connection associated
             * with this file descriptor.
             */
            ForgeConnection *connection =
                forge_connection_manager_get(&connection_manager, fd);

            if (connection == NULL)
            {
                fprintf(stderr, "Connection not found: fd=%d\n", fd);

                continue;
            }

            /*
             * Handle incoming data.
             *
             * The read handler may close and free the connection.
             * If that happens, stop processing this epoll event.
             */
            if (events & EPOLLIN)
            {
                if (forge_client_handle_read(&event_loop, &connection_manager,
                                             connection) <= 0)
                {
                    continue;
                }
            }
            /*
             * The connection may have been removed by the
             * read handler. Verify that it still exists
             * before handling writable events.
             */
            connection = forge_connection_manager_get(&connection_manager, fd);

            if (connection == NULL)
            {
                continue;
            }

            /*
             * Handle outgoing data.
             */
            if (events & EPOLLOUT)
            {
                if (forge_client_handle_write(&event_loop, &connection_manager,
                                              connection) <= 0)
                {
                    continue;
                }
            }
        }
    }

    forge_connection_manager_destroy(&connection_manager);

    forge_event_loop_destroy(&event_loop);

    close(server_fd);

    return 0;
}
