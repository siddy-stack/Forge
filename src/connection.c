#include "connection.h"

#include "protocol.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void forge_connection_compact_output(ForgeConnection *connection)
{
    if (connection->output_offset == 0)
    {
        return;
    }

    if (connection->output_offset >= connection->output_size)
    {
        connection->output_size = 0;
        connection->output_offset = 0;

        return;
    }

    size_t remaining = connection->output_size - connection->output_offset;

    memmove(connection->output_buffer,
            connection->output_buffer + connection->output_offset, remaining);

    connection->output_size = remaining;
    connection->output_offset = 0;
}

void forge_connection_init(ForgeConnection *connection, int file_descriptor)
{
    if (connection == NULL)
    {
        return;
    }

    connection->fd = file_descriptor;

    connection->state = FORGE_CONNECTION_ACCEPTED;
    connection->input_size = 0;

    connection->output_size = 0;
    connection->output_offset = 0;

    forge_connection_touch(connection);
}

void forge_connection_activate(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return;
    }

    if (connection->state != FORGE_CONNECTION_ACCEPTED)
    {
        return;
    }

    connection->state = FORGE_CONNECTION_ACTIVE;
}

void forge_connection_begin_close(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return;
    }

    if (connection->state == FORGE_CONNECTION_CLOSED ||
        connection->state == FORGE_CONNECTION_CLOSING)
    {
        return;
    }

    connection->state = FORGE_CONNECTION_CLOSING;
}

ForgeConnectionState
forge_connection_get_state(const ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return FORGE_CONNECTION_CLOSED;
    }

    return connection->state;
}

void forge_connection_close(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return;
    }

    if (connection->state == FORGE_CONNECTION_CLOSED)
    {
        return;
    }

    connection->state = FORGE_CONNECTION_CLOSING;

    if (connection->fd >= 0)
    {
        close(connection->fd);
        connection->fd = -1;
    }

    connection->input_size = 0;
    connection->output_size = 0;
    connection->output_offset = 0;

    connection->state = FORGE_CONNECTION_CLOSED;
}

uint64_t forge_connection_get_time_ms(void)
{
    struct timespec time;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        return 0;
    }

    return (uint64_t)time.tv_sec * 1000U + (uint64_t)time.tv_nsec / 1000000U;
}

void forge_connection_touch(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return;
    }

    connection->last_activity_ms = forge_connection_get_time_ms();
}

int forge_connection_is_idle(const ForgeConnection *connection, uint64_t now_ms,
                             uint64_t timeout_ms)
{
    if (connection == NULL || connection->state != FORGE_CONNECTION_ACTIVE ||
        now_ms < connection->last_activity_ms)
    {
        return 0;
    }

    return now_ms - connection->last_activity_ms >= timeout_ms;
}

int forge_connection_receive(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return -1;
    }

    if (connection->input_size >= FORGE_CONNECTION_BUFFER_SIZE)
    {
        fprintf(stderr, "Connection input buffer is full.\n");

        return -1;
    }

    size_t available = FORGE_CONNECTION_BUFFER_SIZE - connection->input_size;

    ssize_t bytes_received;

    do
    {
        bytes_received = recv(connection->fd,
                              connection->input_buffer + connection->input_size,
                              available, 0);
    } while (bytes_received == -1 && errno == EINTR);

    if (bytes_received == 0)
    {
        return 0;
    }

    if (bytes_received == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 1;
        }

        fprintf(stderr, "Failed to receive data: %s\n", strerror(errno));

        return -1;
    }

    connection->input_size += (size_t)bytes_received;
    forge_connection_touch(connection);

    return 1;
}

int forge_connection_get_message(const ForgeConnection *connection,
                                 ForgeMessage *message, size_t *bytes_consumed)
{
    if (connection == NULL || message == NULL || bytes_consumed == NULL)
    {
        return -1;
    }

    return forge_protocol_decode((const uint8_t *)connection->input_buffer,
                                 connection->input_size, message,
                                 bytes_consumed);
}

int forge_connection_consume_message(ForgeConnection *connection,
                                     size_t bytes_consumed)
{
    if (connection == NULL)
    {
        return -1;
    }

    if (bytes_consumed > connection->input_size)
    {
        return -1;
    }

    size_t remaining = connection->input_size - bytes_consumed;

    memmove(connection->input_buffer, connection->input_buffer + bytes_consumed,
            remaining);

    connection->input_size = remaining;

    return 0;
}

int forge_connection_queue_send(ForgeConnection *connection, const void *data,
                                size_t length)
{
    if (connection == NULL || data == NULL)
    {
        return -1;
    }

    forge_connection_compact_output(connection);

    if (length > FORGE_CONNECTION_BUFFER_SIZE - connection->output_size)
    {
        fprintf(stderr, "Connection output buffer is full.\n");

        return -1;
    }

    memcpy(connection->output_buffer + connection->output_size, data, length);

    connection->output_size += length;

    return 0;
}

int forge_connection_queue_message(ForgeConnection *connection,
                                   const ForgeMessage *message)
{
    if (connection == NULL || message == NULL)
    {
        return -1;
    }

    forge_connection_compact_output(connection);

    size_t available = FORGE_CONNECTION_BUFFER_SIZE - connection->output_size;

    int encoded_size = forge_protocol_encode(
        message, (uint8_t *)connection->output_buffer + connection->output_size,
        available);

    if (encoded_size == -1)
    {
        return -1;
    }

    connection->output_size += (size_t)encoded_size;

    return 0;
}

int forge_connection_flush(ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return -1;
    }

    while (connection->output_offset < connection->output_size)
    {
        size_t remaining = connection->output_size - connection->output_offset;

        ssize_t bytes_sent =
            send(connection->fd,
                 connection->output_buffer + connection->output_offset,
                 remaining, 0);

        if (bytes_sent == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 1;
            }

            fprintf(stderr, "Failed to send data: %s\n", strerror(errno));

            return -1;
        }

        if (bytes_sent == 0)
        {
            fprintf(stderr, "Failed to send data: connection closed.\n");

            return -1;
        }

        connection->output_offset += (size_t)bytes_sent;
        forge_connection_touch(connection);
    }

    connection->output_size = 0;
    connection->output_offset = 0;

    return 0;
}

int forge_connection_has_pending_output(const ForgeConnection *connection)
{
    if (connection == NULL)
    {
        return 0;
    }

    return connection->output_offset < connection->output_size;
}
