#ifndef FORGE_CONNECTION_H
#define FORGE_CONNECTION_H

#include "protocol.h"

#include <stddef.h>

enum
{
    FORGE_CONNECTION_BUFFER_SIZE =
        FORGE_PROTOCOL_HEADER_SIZE + FORGE_PROTOCOL_MAX_PAYLOAD
};

typedef struct
{
    int fd;

    char input_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t input_size;

    char output_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t output_size;
    size_t output_offset;
} ForgeConnection;

void forge_connection_init(ForgeConnection *connection, int file_descriptor);

void forge_connection_close(ForgeConnection *connection);

int forge_connection_receive(ForgeConnection *connection);

int forge_connection_get_message(const ForgeConnection *connection,
                                 ForgeMessage *message, size_t *bytes_consumed);

int forge_connection_consume_message(ForgeConnection *connection,
                                     size_t bytes_consumed);

int forge_connection_queue_send(ForgeConnection *connection, const void *data,
                                size_t length);

int forge_connection_queue_message(ForgeConnection *connection,
                                   const ForgeMessage *message);

int forge_connection_flush(ForgeConnection *connection);

int forge_connection_has_pending_output(const ForgeConnection *connection);

#endif
