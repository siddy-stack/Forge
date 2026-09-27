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

static void
forge_server_remove_connection(ForgeEventLoop *event_loop,
                               ForgeConnectionManager *connection_manager,
                               int file_descriptor)
{
    forge_event_loop_remove(event_loop, file_descriptor);

    forge_connection_manager_remove(connection_manager, file_descriptor);
}

static void
forge_server_accept_clients(int server_file_descriptor,
                            ForgeEventLoop *event_loop,
                            ForgeConnectionManager *connection_manager)
{
    for (;;)
    {
        int client_file_descriptor =
            forge_network_accept_client(server_file_descriptor);

        if (client_file_descriptor == -1)
        {
            break;
        }

        if (forge_network_set_nonblocking(client_file_descriptor) == -1)
        {
            close(client_file_descriptor);
            continue;
        }

        ForgeConnection *connection = malloc(sizeof(*connection));

        if (connection == NULL)
        {
            fprintf(stderr, "Failed to allocate connection.\n");

            close(client_file_descriptor);
            continue;
        }

        forge_connection_init(connection, client_file_descriptor);

        if (forge_connection_manager_add(connection_manager, connection) == -1)
        {
            forge_connection_close(connection);

            free(connection);
            continue;
        }

        if (forge_event_loop_add(event_loop, client_file_descriptor, EPOLLIN) ==
            -1)
        {
            forge_connection_manager_remove(connection_manager,
                                            client_file_descriptor);

            continue;
        }

        printf("Client connected: fd=%d\n", client_file_descriptor);
    }
}

static int
forge_server_handle_client_events(ForgeEventLoop *event_loop,
                                  ForgeConnectionManager *connection_manager,
                                  const ForgeEvent *event)
{
    int file_descriptor = event->fd;
    uint32_t events = event->events;

    ForgeConnection *connection =
        forge_connection_manager_get(connection_manager, file_descriptor);

    if (connection == NULL)
    {
        fprintf(stderr, "Connection not found: fd=%d\n", file_descriptor);

        return -1;
    }

    if (events & EPOLLIN)
    {
        if (forge_client_handle_read(event_loop, connection_manager,
                                     connection) <= 0)
        {
            return 0;
        }

        connection =
            forge_connection_manager_get(connection_manager, file_descriptor);

        if (connection == NULL)
        {
            return 0;
        }
    }

    if (events & EPOLLOUT)
    {
        if (forge_client_handle_write(event_loop, connection_manager,
                                      connection) <= 0)
        {
            return 0;
        }
    }

    return 0;
}

static void
forge_server_handle_error(ForgeEventLoop *event_loop,
                          ForgeConnectionManager *connection_manager,
                          int server_file_descriptor, int file_descriptor)
{
    if (file_descriptor == server_file_descriptor)
    {
        return;
    }

    forge_server_remove_connection(event_loop, connection_manager,
                                   file_descriptor);
}

static void
forge_server_handle_event(ForgeEventLoop *event_loop,
                          ForgeConnectionManager *connection_manager,
                          int server_file_descriptor, const ForgeEvent *event)
{
    int file_descriptor = event->fd;
    uint32_t events = event->events;

    if (events & (EPOLLERR | EPOLLHUP))
    {
        forge_server_handle_error(event_loop, connection_manager,
                                  server_file_descriptor, file_descriptor);

        return;
    }

    if (file_descriptor == server_file_descriptor)
    {
        forge_server_accept_clients(server_file_descriptor, event_loop,
                                    connection_manager);

        return;
    }

    forge_server_handle_client_events(event_loop, connection_manager, event);
}

static int forge_server_initialize(uint16_t port, int *server_file_descriptor,
                                   ForgeEventLoop *event_loop,
                                   ForgeConnectionManager *connection_manager)
{
    *server_file_descriptor = forge_network_create_listener(port);

    if (*server_file_descriptor == -1)
    {
        return -1;
    }

    if (forge_network_set_nonblocking(*server_file_descriptor) == -1)
    {
        close(*server_file_descriptor);
        return -1;
    }

    if (forge_event_loop_init(event_loop) == -1)
    {
        close(*server_file_descriptor);
        return -1;
    }

    if (forge_event_loop_add(event_loop, *server_file_descriptor, EPOLLIN) ==
        -1)
    {
        forge_event_loop_destroy(event_loop);
        close(*server_file_descriptor);
        return -1;
    }

    forge_connection_manager_init(connection_manager);

    return 0;
}

static void
forge_server_run_event_loop(int server_file_descriptor,
                            ForgeEventLoop *event_loop,
                            ForgeConnectionManager *connection_manager)
{
    for (;;)
    {
        int event_count = forge_event_loop_wait(event_loop, -1);

        if (event_count == -1)
        {
            break;
        }

        for (int index = 0; index < event_count; index++)
        {
            ForgeEvent event;

            if (forge_event_loop_get_event(event_loop, index, &event) == -1)
            {
                continue;
            }

            forge_server_handle_event(event_loop, connection_manager,
                                      server_file_descriptor, &event);
        }
    }
}

static void forge_server_destroy(int server_file_descriptor,
                                 ForgeEventLoop *event_loop,
                                 ForgeConnectionManager *connection_manager)
{
    forge_connection_manager_destroy(connection_manager);

    forge_event_loop_destroy(event_loop);

    close(server_file_descriptor);
}

int forge_server_run(uint16_t port)
{
    int server_file_descriptor = -1;

    ForgeEventLoop event_loop;
    ForgeConnectionManager connection_manager;

    if (forge_server_initialize(port, &server_file_descriptor, &event_loop,
                                &connection_manager) == -1)
    {
        return 1;
    }

    printf("Forge server listening on port %u\n", port);

    forge_server_run_event_loop(server_file_descriptor, &event_loop,
                                &connection_manager);

    forge_server_destroy(server_file_descriptor, &event_loop,
                         &connection_manager);

    return 0;
}
