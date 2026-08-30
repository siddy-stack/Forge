#ifndef FORGE_CONNECTION_H
#define FORGE_CONNECTION_H

#include <stddef.h>

#define FORGE_CONNECTION_BUFFER_SIZE 4096

typedef struct {
    int fd;

    char input_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t input_size;

    char output_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t output_size;
    size_t output_offset;
} ForgeConnection;

void forge_connection_init(
    ForgeConnection *connection,
    int fd
);

void forge_connection_close(
    ForgeConnection *connection
);

int forge_connection_receive(
    ForgeConnection *connection
);

int forge_connection_get_message(
    ForgeConnection *connection,
    char *message,
    size_t message_size
);

int forge_connection_queue_send(
    ForgeConnection *connection,
    const char *data,
    size_t length
);

int forge_connection_flush(
    ForgeConnection *connection
);

int forge_connection_has_pending_output(
    const ForgeConnection *connection
);

#endif