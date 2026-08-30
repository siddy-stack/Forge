#include "connection.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void forge_connection_init(
    ForgeConnection *connection,
    int fd
)
{
    connection->fd = fd;
    connection->input_size = 0;
}

void forge_connection_close(
    ForgeConnection *connection
)
{
    if (connection->fd >= 0) {
        close(connection->fd);
        connection->fd = -1;
    }
}

int forge_connection_receive(
    ForgeConnection *connection
)
{
    if (connection->input_size >=
        FORGE_CONNECTION_BUFFER_SIZE - 1) {
        fprintf(
            stderr,
            "Connection input buffer is full.\n"
        );
        return -1;
    }

    size_t available =
        FORGE_CONNECTION_BUFFER_SIZE -
        connection->input_size -
        1;

    ssize_t bytes_received = recv(
        connection->fd,
        connection->input_buffer +
            connection->input_size,
        available,
        0
    );

    if (bytes_received == 0) {
        return 0;
    }

    if (bytes_received == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }

        fprintf(
            stderr,
            "Failed to receive data: %s\n",
            strerror(errno)
        );
        return -1;
    }

    connection->input_size +=
        (size_t)bytes_received;

    connection->input_buffer[
        connection->input_size
    ] = '\0';

    return 1;
}

int forge_connection_get_message(
    ForgeConnection *connection,
    char *message,
    size_t message_size
)
{
    char *newline = memchr(
        connection->input_buffer,
        '\n',
        connection->input_size
    );

    if (newline == NULL) {
        return 0;
    }

    size_t message_length =
        (size_t)(newline - connection->input_buffer) + 1;

    if (message_length > message_size) {
        return -1;
    }

    memcpy(
        message,
        connection->input_buffer,
        message_length
    );

    message[message_length] = '\0';

    size_t remaining =
        connection->input_size - message_length;

    memmove(
        connection->input_buffer,
        connection->input_buffer + message_length,
        remaining
    );

    connection->input_size = remaining;

    connection->input_buffer[
        connection->input_size
    ] = '\0';

    return 1;
}

int forge_connection_send(
    ForgeConnection *connection,
    const char *data,
    size_t length
)
{
    ssize_t bytes_sent = send(
        connection->fd,
        data,
        length,
        0
    );

    if (bytes_sent == -1) {
        fprintf(
            stderr,
            "Failed to send data: %s\n",
            strerror(errno)
        );
        return -1;
    }

    return (bytes_sent == (ssize_t)length) ? 0 : -1;
}