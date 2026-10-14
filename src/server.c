#include "server.h"

#include "client_handler.h"
#include "connection.h"
#include "connection_manager.h"
#include "event_loop.h"
#include "log.h"
#include "management_api.h"
#include "metrics.h"
#include "network.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

/* NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) */
static volatile sig_atomic_t forge_shutdown_requested = 0;
static int forge_accepting_connections = 1;

enum
{
    FORGE_SERVER_EVENT_WAIT_TIMEOUT_MS = 1000
};

static void forge_server_handle_signal(int signal_number)
{
    (void)signal_number;

    forge_shutdown_requested = 1;
}

static int forge_server_install_signal_handlers(void)
{
    struct sigaction action = {.sa_handler = forge_server_handle_signal,
                               .sa_flags = 0};

    if (sigemptyset(&action.sa_mask) == -1)
    {
        perror("sigemptyset");
        return -1;
    }

    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        perror("sigaction SIGINT");
        return -1;
    }

    if (sigaction(SIGTERM, &action, NULL) == -1)
    {
        perror("sigaction SIGTERM");
        return -1;
    }

    return 0;
}

static void forge_server_remove_connection(
    ForgeEventLoop *event_loop, ForgeConnectionManager *connection_manager,
    ForgeMetrics *metrics, ForgeLogBook *logs, int file_descriptor)
{
    forge_event_loop_remove(event_loop, file_descriptor);

    if (forge_connection_manager_remove(connection_manager, file_descriptor) ==
        0)
    {
        forge_metrics_record_connection_close(metrics);
        forge_log_record(logs, forge_metrics_get_uptime_ms(metrics),
                         "Client closed: fd=%d", file_descriptor);
    }
}

static void
forge_server_accept_clients(int server_file_descriptor,
                            ForgeEventLoop *event_loop,
                            ForgeConnectionManager *connection_manager,
                            ForgeMetrics *metrics, ForgeLogBook *logs)
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
            fprintf(stderr,
                    "Client rejected: connection limit reached for fd=%d\n",
                    client_file_descriptor);
            forge_metrics_record_rejected_connection(metrics);
            forge_log_record(
                logs, forge_metrics_get_uptime_ms(metrics),
                "Client rejected: connection limit reached for fd=%d",
                client_file_descriptor);

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

        forge_connection_activate(connection);
        forge_metrics_record_connection_open(metrics);
        forge_log_record(logs, forge_metrics_get_uptime_ms(metrics),
                         "Client connected: fd=%d", client_file_descriptor);

        printf("Client connected: fd=%d\n", client_file_descriptor);
    }
}

static void
forge_server_remove_idle_connections(ForgeEventLoop *event_loop,
                                     ForgeConnectionManager *connection_manager,
                                     ForgeMetrics *metrics, ForgeLogBook *logs)
{
    uint64_t now_ms = forge_connection_get_time_ms();

    for (size_t index = 0; index < FORGE_MAX_CONNECTIONS; index++)
    {
        const ForgeConnection *connection =
            connection_manager->connections[index];

        if (forge_connection_is_idle(connection, now_ms,
                                     FORGE_CONNECTION_IDLE_TIMEOUT_MS))
        {
            int file_descriptor = connection->fd;

            printf("Client timed out: fd=%d\n", file_descriptor);

            forge_log_record(logs, forge_metrics_get_uptime_ms(metrics),
                             "Client timed out: fd=%d", file_descriptor);
            forge_server_remove_connection(event_loop, connection_manager,
                                           metrics, logs, file_descriptor);
        }
    }
}

static int forge_server_handle_client_events(
    ForgeEventLoop *event_loop, ForgeConnectionManager *connection_manager,
    ForgeMetrics *metrics, ForgeLogBook *logs, const ForgeEvent *event)
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
        if (forge_client_handle_read(event_loop, connection_manager, metrics,
                                     logs, connection) <= 0)
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
        if (forge_client_handle_write(event_loop, connection_manager, metrics,
                                      logs, connection) <= 0)
        {
            return 0;
        }
    }

    return 0;
}

static void
forge_server_handle_error(ForgeEventLoop *event_loop,
                          ForgeConnectionManager *connection_manager,
                          ForgeMetrics *metrics, ForgeLogBook *logs,
                          int server_file_descriptor, int file_descriptor)
{
    if (file_descriptor == server_file_descriptor)
    {
        return;
    }

    forge_server_remove_connection(event_loop, connection_manager, metrics,
                                   logs, file_descriptor);
}

static void
forge_server_handle_event(ForgeEventLoop *event_loop,
                          ForgeConnectionManager *connection_manager,
                          ForgeMetrics *metrics, ForgeLogBook *logs,
                          ForgeManagementApi *management_api,
                          int server_file_descriptor, const ForgeEvent *event)
{
    int file_descriptor = event->fd;
    uint32_t events = event->events;
    int disconnect_file_descriptor = -1;

    if (forge_management_api_handle_event(
            management_api, event_loop, event, metrics, connection_manager,
            logs, &forge_shutdown_requested, &forge_accepting_connections,
            &disconnect_file_descriptor))
    {
        if (disconnect_file_descriptor >= 0)
        {
            forge_server_remove_connection(event_loop, connection_manager,
                                           metrics, logs,
                                           disconnect_file_descriptor);
        }
        return;
    }

    if (events & (EPOLLERR | EPOLLHUP))
    {
        forge_server_handle_error(event_loop, connection_manager, metrics, logs,
                                  server_file_descriptor, file_descriptor);

        return;
    }

    if (file_descriptor == server_file_descriptor &&
        forge_accepting_connections)
    {
        forge_server_accept_clients(server_file_descriptor, event_loop,
                                    connection_manager, metrics, logs);

        return;
    }

    forge_server_handle_client_events(event_loop, connection_manager, metrics,
                                      logs, event);
}

static int forge_server_initialize(uint16_t port, int *server_file_descriptor,
                                   ForgeEventLoop *event_loop,
                                   ForgeConnectionManager *connection_manager,
                                   ForgeManagementApi *management_api)
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
    forge_management_api_init(management_api);

    if (forge_management_api_start(management_api, event_loop,
                                   FORGE_DEFAULT_MANAGEMENT_PORT) == -1)
    {
        forge_event_loop_destroy(event_loop);
        close(*server_file_descriptor);
        return -1;
    }

    return 0;
}

static void forge_server_run_event_loop(
    int server_file_descriptor, ForgeEventLoop *event_loop,
    ForgeConnectionManager *connection_manager, ForgeMetrics *metrics,
    ForgeLogBook *logs, ForgeManagementApi *management_api)
{
    ForgeMetrics previous_metrics = *metrics;

    while (!forge_shutdown_requested)
    {
        int event_count = forge_event_loop_wait(
            event_loop, FORGE_SERVER_EVENT_WAIT_TIMEOUT_MS);

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

            forge_server_handle_event(event_loop, connection_manager, metrics,
                                      logs, management_api,
                                      server_file_descriptor, &event);
        }

        forge_server_remove_idle_connections(event_loop, connection_manager,
                                             metrics, logs);
        if (metrics->total_connections > previous_metrics.total_connections)
        {
            forge_management_api_publish_event(management_api, event_loop,
                                               metrics, "connection_opened");
        }
        if (metrics->active_connections < previous_metrics.active_connections)
        {
            forge_management_api_publish_event(management_api, event_loop,
                                               metrics, "connection_closed");
        }
        if (metrics->messages_received > previous_metrics.messages_received)
        {
            forge_management_api_publish_event(management_api, event_loop,
                                               metrics, "message_received");
        }
        if (metrics->protocol_errors > previous_metrics.protocol_errors)
        {
            forge_management_api_publish_event(management_api, event_loop,
                                               metrics, "protocol_error");
            forge_management_api_publish_event(management_api, event_loop,
                                               metrics, "server_warning");
        }
        forge_management_api_publish_metrics(management_api, event_loop,
                                             metrics);
        previous_metrics = *metrics;
    }
}

static void forge_server_destroy(int server_file_descriptor,
                                 ForgeEventLoop *event_loop,
                                 ForgeConnectionManager *connection_manager,
                                 ForgeMetrics *metrics, ForgeLogBook *logs,
                                 ForgeManagementApi *management_api)
{
    for (size_t index = 0; index < FORGE_MAX_CONNECTIONS; index++)
    {
        const ForgeConnection *connection =
            connection_manager->connections[index];

        if (connection != NULL)
        {
            forge_server_remove_connection(event_loop, connection_manager,
                                           metrics, logs, connection->fd);
        }
    }

    forge_connection_manager_destroy(connection_manager);

    forge_management_api_stop(management_api, event_loop);

    forge_event_loop_destroy(event_loop);

    close(server_file_descriptor);

    forge_metrics_print(metrics);
}

int forge_server_run(uint16_t port)
{
    int server_file_descriptor = -1;

    ForgeEventLoop event_loop;
    ForgeConnectionManager connection_manager;
    ForgeMetrics metrics;
    ForgeLogBook logs;
    ForgeManagementApi management_api;

    forge_shutdown_requested = 0;
    forge_accepting_connections = 1;
    forge_metrics_init(&metrics);
    forge_log_init(&logs);

    if (forge_server_install_signal_handlers() == -1)
    {
        return 1;
    }

    if (forge_server_initialize(port, &server_file_descriptor, &event_loop,
                                &connection_manager, &management_api) == -1)
    {
        return 1;
    }

    printf("Forge server listening on port %u\n", port);
    printf("Forge management API listening on http://127.0.0.1:%u\n",
           (unsigned int)FORGE_DEFAULT_MANAGEMENT_PORT);
    forge_log_record(&logs, forge_metrics_get_uptime_ms(&metrics),
                     "Forge server started");

    forge_server_run_event_loop(server_file_descriptor, &event_loop,
                                &connection_manager, &metrics, &logs,
                                &management_api);

    printf("Forge server shutting down...\n");
    forge_log_record(&logs, forge_metrics_get_uptime_ms(&metrics),
                     "Forge server shutting down");

    forge_server_destroy(server_file_descriptor, &event_loop,
                         &connection_manager, &metrics, &logs, &management_api);

    printf("Forge server stopped.\n");

    return 0;
}
