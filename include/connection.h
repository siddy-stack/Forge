#ifndef FORGE_CONNECTION_H
#define FORGE_CONNECTION_H

#include <stddef.h>

#define FORGE_CONNECTION_BUFFER_SIZE 4096

typedef struct {
    int fd;
    char buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t bytes_received;
} ForgeConnection;

void forge_connection_init(ForgeConnection *connection, int fd);

void forge_connection_close(ForgeConnection *connection);

int forge_connection_receive(ForgeConnection *connection);

int forge_connection_send(
    ForgeConnection *connection,
    const char *data,
    size_t length
);

#endif