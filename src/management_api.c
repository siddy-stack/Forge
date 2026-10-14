#include "management_api.h"

#include "connection.h"
#include "dashboard.h"
#include "network.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct
{
    char *buffer;
    size_t capacity;
    size_t length;
    bool failed;
} ForgeManagementWriter;

typedef struct
{
    int code;
    const char *text;
} ForgeHttpStatus;

typedef struct
{
    const char *content_type;
    const char *body;
} ForgeHttpResponseContent;

static void forge_management_writer_append(ForgeManagementWriter *writer,
                                           const char *format, ...)
{
    va_list arguments;
    int written;

    if (writer == NULL || writer->failed)
    {
        return;
    }

    va_start(arguments, format);
    /* NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized) */
    written = vsnprintf(writer->buffer + writer->length,
                        writer->capacity - writer->length, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= writer->capacity - writer->length)
    {
        writer->failed = true;
        return;
    }

    writer->length += (size_t)written;
}

static void
forge_management_writer_append_json_string(ForgeManagementWriter *writer,
                                           const char *value)
{
    if (writer == NULL || value == NULL)
    {
        return;
    }

    forge_management_writer_append(writer, "\"");

    for (size_t index = 0; value[index] != '\0'; index++)
    {
        unsigned char character = (unsigned char)value[index];

        if (character == '\"' || character == '\\')
        {
            forge_management_writer_append(writer, "\\%c", character);
        }
        else if (character == '\n')
        {
            forge_management_writer_append(writer, "\\n");
        }
        else if (character == '\r')
        {
            forge_management_writer_append(writer, "\\r");
        }
        else if (character == '\t')
        {
            forge_management_writer_append(writer, "\\t");
        }
        else if (character < 0x20U)
        {
            forge_management_writer_append(writer, "\\u%04x", character);
        }
        else
        {
            forge_management_writer_append(writer, "%c", character);
        }
    }

    forge_management_writer_append(writer, "\"");
}

static const char *
forge_management_connection_state_name(ForgeConnectionState state)
{
    switch (state)
    {
    case FORGE_CONNECTION_ACCEPTED:
        return "accepted";
    case FORGE_CONNECTION_ACTIVE:
        return "active";
    case FORGE_CONNECTION_CLOSING:
        return "closing";
    case FORGE_CONNECTION_CLOSED:
        return "closed";
    }

    return "unknown";
}

static void
forge_management_build_status(ForgeManagementWriter *writer,
                              const ForgeMetrics *metrics,
                              const ForgeConnectionManager *connections)
{
    size_t connection_count = connections == NULL ? 0U : connections->count;
    uint64_t total_connections =
        metrics == NULL ? 0U : metrics->total_connections;

    forge_management_writer_append(
        writer,
        "{\"status\":\"running\",\"service\":\"forge\",\"uptime_seconds\":"
        "%" PRIu64 ",\"uptime_ms\":%" PRIu64 ",\"active_connections\":%zu"
        ",\"total_connections\":%" PRIu64 "}",
        forge_metrics_get_uptime_ms(metrics) / 1000U,
        forge_metrics_get_uptime_ms(metrics), connection_count,
        total_connections);
}

static void forge_management_build_metrics(ForgeManagementWriter *writer,
                                           const ForgeMetrics *metrics)
{
    ForgeMetrics empty_metrics = {0};
    const ForgeMetrics *reported_metrics = metrics;

    if (reported_metrics == NULL)
    {
        reported_metrics = &empty_metrics;
    }

    forge_management_writer_append(
        writer,
        "{\"uptime_seconds\":%" PRIu64 ",\"uptime_ms\":%" PRIu64
        ",\"total_connections\":%" PRIu64 ",\"active_connections\":%" PRIu64
        ",\"messages_received\":%" PRIu64 ",\"messages_sent\":%" PRIu64
        ",\"bytes_received\":%" PRIu64 ",\"bytes_sent\":%" PRIu64
        ",\"rejected_connections\":%" PRIu64 ",\"protocol_errors\":%" PRIu64
        "}",
        forge_metrics_get_uptime_ms(reported_metrics) / 1000U,
        forge_metrics_get_uptime_ms(reported_metrics),
        reported_metrics->total_connections,
        reported_metrics->active_connections,
        reported_metrics->messages_received, reported_metrics->messages_sent,
        reported_metrics->bytes_received, reported_metrics->bytes_sent,
        reported_metrics->rejected_connections,
        reported_metrics->protocol_errors);
}

static void
forge_management_build_connections(ForgeManagementWriter *writer,
                                   const ForgeConnectionManager *connections)
{
    uint64_t now_ms = forge_connection_get_time_ms();
    size_t connection_count = connections == NULL ? 0U : connections->count;
    forge_management_writer_append(writer, "{\"count\":%zu,\"connections\":[",
                                   connection_count);

    if (connections != NULL)
    {
        bool is_first = true;

        for (size_t index = 0; index < FORGE_MAX_CONNECTIONS; index++)
        {
            const ForgeConnection *connection = connections->connections[index];
            uint64_t idle_ms;
            size_t pending_output;

            if (connection == NULL)
            {
                continue;
            }

            if (!is_first)
            {
                forge_management_writer_append(writer, ",");
            }

            is_first = false;
            idle_ms = now_ms < connection->last_activity_ms
                          ? 0U
                          : now_ms - connection->last_activity_ms;
            pending_output =
                connection->output_size - connection->output_offset;

            forge_management_writer_append(
                writer,
                "{\"fd\":%d,\"state\":\"%s\",\"input_bytes\":%zu,"
                "\"pending_output_bytes\":%zu,\"idle_ms\":%" PRIu64 "}",
                connection->fd,
                forge_management_connection_state_name(connection->state),
                connection->input_size, pending_output, idle_ms);
        }
    }

    forge_management_writer_append(writer, "]}");
}

static int
forge_management_build_connection(ForgeManagementWriter *writer,
                                  const ForgeConnectionManager *connections,
                                  int file_descriptor)
{
    uint64_t now_ms = forge_connection_get_time_ms();

    if (connections == NULL)
    {
        return -1;
    }

    for (size_t index = 0; index < FORGE_MAX_CONNECTIONS; index++)
    {
        const ForgeConnection *connection = connections->connections[index];
        uint64_t idle_ms;
        size_t pending_output;

        if (connection == NULL || connection->fd != file_descriptor)
        {
            continue;
        }

        idle_ms = now_ms < connection->last_activity_ms
                      ? 0U
                      : now_ms - connection->last_activity_ms;
        pending_output = connection->output_size - connection->output_offset;
        forge_management_writer_append(
            writer,
            "{\"fd\":%d,\"state\":\"%s\",\"input_bytes\":%zu,"
            "\"pending_output_bytes\":%zu,\"idle_ms\":%" PRIu64 "}",
            connection->fd,
            forge_management_connection_state_name(connection->state),
            connection->input_size, pending_output, idle_ms);
        return writer->failed ? -1 : 0;
    }

    return -1;
}

static int forge_management_parse_connection_fd(const char *path,
                                                int *file_descriptor)
{
    static const char prefix[] = "/api/connections/";
    const char *value;
    char *end;
    long parsed;

    if (path == NULL || file_descriptor == NULL ||
        strncmp(path, prefix, sizeof(prefix) - 1U) != 0)
    {
        return 0;
    }

    value = path + sizeof(prefix) - 1U;
    if (*value == '\0')
    {
        return -1;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < 0 || parsed > INT_MAX)
    {
        return -1;
    }

    *file_descriptor = (int)parsed;
    return 1;
}

static void forge_management_build_logs(ForgeManagementWriter *writer,
                                        const ForgeLogBook *logs)
{
    size_t count = logs == NULL ? 0U : logs->count;

    forge_management_writer_append(writer, "{\"count\":%zu,\"logs\":[", count);

    for (size_t index = 0; index < count; index++)
    {
        const ForgeLogEntry *entry = forge_log_get(logs, index);

        if (entry == NULL)
        {
            continue;
        }

        if (index > 0U)
        {
            forge_management_writer_append(writer, ",");
        }

        forge_management_writer_append(
            writer,
            "{\"uptime_ms\":%" PRIu64 ",\"message\":", entry->uptime_ms);
        forge_management_writer_append_json_string(writer, entry->message);
        forge_management_writer_append(writer, "}");
    }

    forge_management_writer_append(writer, "]}");
}

static int
forge_management_write_http_response(char *response, size_t response_size,
                                     ForgeHttpStatus status,
                                     ForgeHttpResponseContent content)
{
    int header_size;
    size_t body_size = strlen(content.body);

    header_size =
        snprintf(response, response_size,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 status.code, status.text, content.content_type, body_size);

    if (header_size < 0 || (size_t)header_size >= response_size ||
        body_size >= response_size - (size_t)header_size)
    {
        return -1;
    }

    memcpy(response + (size_t)header_size, content.body, body_size + 1U);

    return header_size + (int)body_size;
}

int forge_management_api_build_response(
    char *response, size_t response_size, const char *method, const char *path,
    const ForgeMetrics *metrics, const ForgeConnectionManager *connections,
    const ForgeLogBook *logs)
{
    char body[FORGE_MANAGEMENT_RESPONSE_SIZE];
    ForgeManagementWriter writer = {
        .buffer = body, .capacity = sizeof(body), .length = 0, .failed = false};
    int status_code = 200;
    const char *status_text = "OK";
    int connection_fd = -1;

    if (response == NULL || response_size == 0U || method == NULL ||
        path == NULL)
    {
        return -1;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/status") == 0)
    {
        forge_management_build_status(&writer, metrics, connections);
    }
    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/api/connections") == 0)
    {
        forge_management_build_connections(&writer, connections);
    }
    else if (strcmp(method, "GET") == 0 &&
             forge_management_parse_connection_fd(path, &connection_fd) == 1)
    {
        if (forge_management_build_connection(&writer, connections,
                                              connection_fd) == -1)
        {
            status_code = 404;
            status_text = "Not Found";
            forge_management_writer_append(
                &writer, "{\"error\":\"connection not found\"}");
        }
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/metrics") == 0)
    {
        forge_management_build_metrics(&writer, metrics);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/logs") == 0)
    {
        forge_management_build_logs(&writer, logs);
    }
    else if (strcmp(method, "POST") == 0 &&
             strcmp(path, "/api/control/stop") == 0)
    {
        forge_management_writer_append(&writer, "{\"status\":\"stopping\"}");
    }
    else if (strcmp(method, "GET") != 0)
    {
        status_code = 405;
        status_text = "Method Not Allowed";
        forge_management_writer_append(&writer,
                                       "{\"error\":\"method not allowed\"}");
    }
    else
    {
        status_code = 404;
        status_text = "Not Found";
        forge_management_writer_append(&writer, "{\"error\":\"not found\"}");
    }

    if (writer.failed)
    {
        return -1;
    }

    body[writer.length] = '\0';

    return forge_management_write_http_response(
        response, response_size,
        (ForgeHttpStatus){.code = status_code, .text = status_text},
        (ForgeHttpResponseContent){.content_type = "application/json",
                                   .body = body});
}

static int forge_management_api_build_dashboard_response(char *response,
                                                         size_t response_size)
{
    return forge_management_write_http_response(
        response, response_size, (ForgeHttpStatus){.code = 200, .text = "OK"},
        (ForgeHttpResponseContent){.content_type = "text/html; charset=utf-8",
                                   .body = forge_dashboard_html()});
}

static ForgeManagementClient *
forge_management_api_find_client(ForgeManagementApi *api, int file_descriptor)
{
    if (api == NULL)
    {
        return NULL;
    }

    for (size_t index = 0; index < FORGE_MANAGEMENT_MAX_CLIENTS; index++)
    {
        if (api->clients[index].fd == file_descriptor)
        {
            return &api->clients[index];
        }
    }

    return NULL;
}

static void forge_management_api_remove_client(ForgeEventLoop *event_loop,
                                               ForgeManagementClient *client)
{
    if (client == NULL || client->fd < 0)
    {
        return;
    }

    forge_event_loop_remove(event_loop, client->fd);
    close(client->fd);
    client->fd = -1;
    client->request_size = 0;
    client->response_size = 0;
    client->response_offset = 0;
    client->shutdown_after_response = 0;
}

static int forge_management_api_prepare_response(
    ForgeManagementClient *client, const ForgeMetrics *metrics,
    const ForgeConnectionManager *connections, const ForgeLogBook *logs)
{
    char method[8];
    char path[256];
    int parsed_fields;
    int response_size;

    client->shutdown_after_response = 0;
    client->request[client->request_size] = '\0';
    parsed_fields = sscanf(client->request, "%7s %255s", method, path);

    if (parsed_fields != 2)
    {
        response_size = forge_management_api_build_response(
            client->response, sizeof(client->response), "", "", metrics,
            connections, logs);
    }
    else
    {
        char *query = strchr(path, '?');

        if (query != NULL)
        {
            *query = '\0';
        }

        if (strcmp(method, "GET") == 0 &&
            (strcmp(path, "/") == 0 || strcmp(path, "/dashboard") == 0))
        {
            response_size = forge_management_api_build_dashboard_response(
                client->response, sizeof(client->response));
        }
        else
        {
            response_size = forge_management_api_build_response(
                client->response, sizeof(client->response), method, path,
                metrics, connections, logs);
        }

        client->shutdown_after_response =
            strcmp(method, "POST") == 0 &&
            strcmp(path, "/api/control/stop") == 0;
    }

    if (response_size < 0)
    {
        return -1;
    }

    client->response_size = (size_t)response_size;
    client->response_offset = 0;

    return 0;
}

static int
forge_management_request_is_complete(const ForgeManagementClient *client)
{
    return client != NULL && strstr(client->request, "\r\n\r\n") != NULL;
}

static void forge_management_api_accept_clients(ForgeManagementApi *api,
                                                ForgeEventLoop *event_loop)
{
    for (;;)
    {
        int client_file_descriptor =
            forge_network_accept_client(api->listener_fd);
        ForgeManagementClient *client = NULL;

        if (client_file_descriptor == -1)
        {
            return;
        }

        for (size_t index = 0; index < FORGE_MANAGEMENT_MAX_CLIENTS; index++)
        {
            if (api->clients[index].fd < 0)
            {
                client = &api->clients[index];
                break;
            }
        }

        if (client == NULL ||
            forge_network_set_nonblocking(client_file_descriptor) == -1 ||
            forge_event_loop_add(event_loop, client_file_descriptor, EPOLLIN) ==
                -1)
        {
            close(client_file_descriptor);
            continue;
        }

        client->fd = client_file_descriptor;
        client->request_size = 0;
        client->response_size = 0;
        client->response_offset = 0;
        client->shutdown_after_response = 0;
    }
}

static void forge_management_api_handle_read(
    ForgeManagementClient *client, ForgeEventLoop *event_loop,
    const ForgeMetrics *metrics, const ForgeConnectionManager *connections,
    const ForgeLogBook *logs)
{
    for (;;)
    {
        ssize_t received =
            recv(client->fd, client->request + client->request_size,
                 sizeof(client->request) - 1U - client->request_size, 0);

        if (received > 0)
        {
            client->request_size += (size_t)received;
            client->request[client->request_size] = '\0';

            if (forge_management_request_is_complete(client))
            {
                if (forge_management_api_prepare_response(
                        client, metrics, connections, logs) == -1 ||
                    forge_event_loop_modify(event_loop, client->fd, EPOLLOUT) ==
                        -1)
                {
                    forge_management_api_remove_client(event_loop, client);
                }

                return;
            }

            if (client->request_size == sizeof(client->request) - 1U)
            {
                forge_management_api_remove_client(event_loop, client);
                return;
            }

            continue;
        }

        if (received == 0)
        {
            forge_management_api_remove_client(event_loop, client);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }

        forge_management_api_remove_client(event_loop, client);
        return;
    }
}

static void
forge_management_api_handle_write(ForgeManagementClient *client,
                                  ForgeEventLoop *event_loop,
                                  volatile sig_atomic_t *shutdown_requested)
{
    while (client->response_offset < client->response_size)
    {
        ssize_t sent =
            send(client->fd, client->response + client->response_offset,
                 client->response_size - client->response_offset, MSG_NOSIGNAL);

        if (sent > 0)
        {
            client->response_offset += (size_t)sent;
            continue;
        }

        if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }

        forge_management_api_remove_client(event_loop, client);
        return;
    }

    if (client->shutdown_after_response != 0 && shutdown_requested != NULL)
    {
        *shutdown_requested = 1;
    }

    forge_management_api_remove_client(event_loop, client);
}

void forge_management_api_init(ForgeManagementApi *api)
{
    if (api == NULL)
    {
        return;
    }

    memset(api, 0, sizeof(*api));
    api->listener_fd = -1;

    for (size_t index = 0; index < FORGE_MANAGEMENT_MAX_CLIENTS; index++)
    {
        api->clients[index].fd = -1;
    }
}

int forge_management_api_start(ForgeManagementApi *api,
                               ForgeEventLoop *event_loop, uint16_t port)
{
    if (api == NULL || event_loop == NULL)
    {
        return -1;
    }

    api->listener_fd = forge_network_create_local_listener(port);

    if (api->listener_fd == -1 ||
        forge_network_set_nonblocking(api->listener_fd) == -1 ||
        forge_event_loop_add(event_loop, api->listener_fd, EPOLLIN) == -1)
    {
        if (api->listener_fd >= 0)
        {
            close(api->listener_fd);
            api->listener_fd = -1;
        }

        return -1;
    }

    api->port = port;

    return 0;
}

void forge_management_api_stop(ForgeManagementApi *api,
                               ForgeEventLoop *event_loop)
{
    if (api == NULL || event_loop == NULL)
    {
        return;
    }

    for (size_t index = 0; index < FORGE_MANAGEMENT_MAX_CLIENTS; index++)
    {
        forge_management_api_remove_client(event_loop, &api->clients[index]);
    }

    if (api->listener_fd >= 0)
    {
        forge_event_loop_remove(event_loop, api->listener_fd);
        close(api->listener_fd);
        api->listener_fd = -1;
    }
}

int forge_management_api_handles_fd(const ForgeManagementApi *api,
                                    int file_descriptor)
{
    if (api == NULL)
    {
        return 0;
    }

    if (api->listener_fd == file_descriptor)
    {
        return 1;
    }

    for (size_t index = 0; index < FORGE_MANAGEMENT_MAX_CLIENTS; index++)
    {
        if (api->clients[index].fd == file_descriptor)
        {
            return 1;
        }
    }

    return 0;
}

int forge_management_api_handle_event(ForgeManagementApi *api,
                                      ForgeEventLoop *event_loop,
                                      const ForgeEvent *event,
                                      const ForgeMetrics *metrics,
                                      const ForgeConnectionManager *connections,
                                      const ForgeLogBook *logs,
                                      volatile sig_atomic_t *shutdown_requested)
{
    ForgeManagementClient *client;

    if (api == NULL || event_loop == NULL || event == NULL ||
        !forge_management_api_handles_fd(api, event->fd))
    {
        return 0;
    }

    if (event->fd == api->listener_fd)
    {
        if (event->events & EPOLLIN)
        {
            forge_management_api_accept_clients(api, event_loop);
        }

        return 1;
    }

    client = forge_management_api_find_client(api, event->fd);

    if (client == NULL)
    {
        return 0;
    }

    if (event->events & (EPOLLERR | EPOLLHUP))
    {
        forge_management_api_remove_client(event_loop, client);
        return 1;
    }

    if (event->events & EPOLLIN)
    {
        forge_management_api_handle_read(client, event_loop, metrics,
                                         connections, logs);
    }

    if (client->fd >= 0 && event->events & EPOLLOUT)
    {
        forge_management_api_handle_write(client, event_loop,
                                          shutdown_requested);
    }

    return 1;
}
