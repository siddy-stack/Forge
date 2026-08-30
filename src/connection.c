#include "connection.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void forge_connection_init(ForgeConnection *connection, int fd)
{
    connection->fd = fd;
    connection->bytes_received = 0;
}

void forge_connection_close(ForgeConnection *connection)
{
    if (connection->fd >= 0) {
        close(connection->fd);
        connection->fd = -1;
    }
}

int forge_connection_receive(ForgeConnection *connection)
{
    ssize_t bytes_received = recv(
        connection->fd,
        connection->buffer,
        sizeof(connection->buffer) - 1,
        0
    );

    if (bytes_received == 0) {
        return 0;
    }

    if (bytes_received == -1) {
        fprintf(
            stderr,
            "Failed to receive data: %s\n",
            strerror(errno)
        );
        return -1;
    }

    connection->buffer[bytes_received] = '\0';
    connection->bytes_received = (size_t)bytes_received;

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