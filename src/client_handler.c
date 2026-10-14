#include "client_handler.h"

#include "message_dispatcher.h"

#include <stdio.h>
#include <sys/epoll.h>

static int forge_client_update_events(ForgeEventLoop *event_loop,
                                      const ForgeConnection *connection)
{
    uint32_t events = EPOLLIN;

    if (forge_connection_has_pending_output(connection))
    {
        events |= EPOLLOUT;
    }

    return forge_event_loop_modify(event_loop, connection->fd, events);
}

static int forge_client_disconnect(ForgeEventLoop *event_loop,
                                   ForgeConnectionManager *connection_manager,
                                   ForgeMetrics *metrics, ForgeLogBook *logs,
                                   int file_descriptor)
{
    forge_event_loop_remove(event_loop, file_descriptor);

    if (forge_connection_manager_remove(connection_manager, file_descriptor) ==
        -1)
    {
        return -1;
    }

    forge_metrics_record_connection_close(metrics);
    forge_log_record(logs, forge_metrics_get_uptime_ms(metrics),
                     "Client disconnected: fd=%d", file_descriptor);

    return 0;
}

int forge_client_handle_read(ForgeEventLoop *event_loop,
                             ForgeConnectionManager *connection_manager,
                             ForgeMetrics *metrics, ForgeLogBook *logs,
                             ForgeConnection *connection)
{
    int fd = connection->fd;
    size_t input_size_before = connection->input_size;

    int result = forge_connection_receive(connection);

    if (result == 0)
    {
        forge_client_disconnect(event_loop, connection_manager, metrics, logs,
                                fd);

        printf("Client disconnected: fd=%d\n", fd);

        return 0;
    }

    if (result == -1)
    {
        forge_client_disconnect(event_loop, connection_manager, metrics, logs,
                                fd);

        return -1;
    }

    forge_metrics_record_bytes_received(
        metrics, (uint64_t)(connection->input_size - input_size_before));

    for (;;)
    {
        ForgeMessage message;
        size_t bytes_consumed = 0;

        int message_result =
            forge_connection_get_message(connection, &message, &bytes_consumed);

        if (message_result == 0)
        {
            break;
        }

        if (message_result == -1)
        {
            fprintf(stderr,
                    "Invalid Forge protocol message "
                    "from fd=%d\n",
                    fd);
            forge_metrics_record_protocol_error(metrics);

            forge_client_disconnect(event_loop, connection_manager, metrics,
                                    logs, fd);

            return -1;
        }

        printf("Received Forge message: "
               "fd=%d type=%u request_id=%u "
               "payload=%u bytes\n",
               fd, message.type, message.request_id, message.payload_length);

        forge_metrics_record_message_received(metrics);
        forge_log_record(logs, forge_metrics_get_uptime_ms(metrics),
                         "Received message: fd=%d type=%u request_id=%u", fd,
                         message.type, message.request_id);

        if (forge_message_dispatch(connection, &message) == -1)
        {
            forge_client_disconnect(event_loop, connection_manager, metrics,
                                    logs, fd);

            return -1;
        }

        forge_metrics_record_message_sent(metrics);

        if (forge_connection_consume_message(connection, bytes_consumed) == -1)
        {
            forge_client_disconnect(event_loop, connection_manager, metrics,
                                    logs, fd);

            return -1;
        }
    }

    if (forge_client_update_events(event_loop, connection) == -1)
    {
        forge_client_disconnect(event_loop, connection_manager, metrics, logs,
                                fd);

        return -1;
    }

    return 1;
}

int forge_client_handle_write(ForgeEventLoop *event_loop,
                              ForgeConnectionManager *connection_manager,
                              ForgeMetrics *metrics, ForgeLogBook *logs,
                              ForgeConnection *connection)
{
    int fd = connection->fd;
    size_t output_size_before =
        connection->output_size - connection->output_offset;

    int result = forge_connection_flush(connection);
    size_t output_size_after =
        connection->output_size - connection->output_offset;

    forge_metrics_record_bytes_sent(
        metrics, (uint64_t)(output_size_before - output_size_after));

    if (result == -1)
    {
        forge_client_disconnect(event_loop, connection_manager, metrics, logs,
                                fd);

        return -1;
    }

    if (forge_client_update_events(event_loop, connection) == -1)
    {
        forge_client_disconnect(event_loop, connection_manager, metrics, logs,
                                fd);

        return -1;
    }

    return 1;
}
