#ifndef FORGE_CONNECTION_H
#define FORGE_CONNECTION_H

#include "protocol.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    FORGE_CONNECTION_BUFFER_SIZE =
        FORGE_PROTOCOL_HEADER_SIZE + FORGE_PROTOCOL_MAX_PAYLOAD,
    FORGE_CONNECTION_IDLE_TIMEOUT_MS = 60000
};

typedef enum
{
    FORGE_CONNECTION_ACCEPTED,
    FORGE_CONNECTION_ACTIVE,
    FORGE_CONNECTION_CLOSING,
    FORGE_CONNECTION_CLOSED
} ForgeConnectionState;

typedef struct
{
    int fd;
    ForgeConnectionState state;

    char input_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t input_size;

    char output_buffer[FORGE_CONNECTION_BUFFER_SIZE];
    size_t output_size;
    size_t output_offset;

    uint64_t last_activity_ms;
} ForgeConnection;

void forge_connection_init(ForgeConnection *connection, int file_descriptor);

void forge_connection_activate(ForgeConnection *connection);

void forge_connection_begin_close(ForgeConnection *connection);

ForgeConnectionState
forge_connection_get_state(const ForgeConnection *connection);

void forge_connection_close(ForgeConnection *connection);

uint64_t forge_connection_get_time_ms(void);

void forge_connection_touch(ForgeConnection *connection);

int forge_connection_is_idle(const ForgeConnection *connection, uint64_t now_ms,
                             uint64_t timeout_ms);

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
