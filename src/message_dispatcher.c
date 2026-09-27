#include "message_dispatcher.h"

#include "message.h"

#include <stdio.h>

static int forge_dispatch_ping(ForgeConnection *connection,
                               const ForgeMessage *message)
{
    ForgeMessage response = {.type = FORGE_MESSAGE_PONG,
                             .flags = message->flags,
                             .request_id = message->request_id,
                             .payload = message->payload,
                             .payload_length = message->payload_length};

    printf("Dispatching PING: request_id=%u\n", message->request_id);

    return forge_connection_queue_message(connection, &response);
}

static int forge_dispatch_connect(ForgeConnection *connection,
                                  const ForgeMessage *message)
{
    const char payload[] = "connected";

    ForgeMessage response = {.type = FORGE_MESSAGE_CONNECT,
                             .flags = 0,
                             .request_id = message->request_id,
                             .payload = (const uint8_t *)payload,
                             .payload_length = sizeof(payload) - 1};

    printf("Dispatching CONNECT: request_id=%u\n", message->request_id);

    return forge_connection_queue_message(connection, &response);
}

static int forge_dispatch_disconnect(ForgeConnection *connection,
                                     const ForgeMessage *message)
{
    printf("Dispatching DISCONNECT: request_id=%u\n", message->request_id);

    ForgeMessage response = {.type = FORGE_MESSAGE_DISCONNECT,
                             .flags = 0,
                             .request_id = message->request_id,
                             .payload = NULL,
                             .payload_length = 0};

    return forge_connection_queue_message(connection, &response);
}

static int forge_dispatch_error(ForgeConnection *connection,
                                const ForgeMessage *message)
{
    const char payload[] = "unknown message type";

    ForgeMessage response = {.type = FORGE_MESSAGE_ERROR,
                             .flags = 0,
                             .request_id = message->request_id,
                             .payload = (const uint8_t *)payload,
                             .payload_length = sizeof(payload) - 1};

    return forge_connection_queue_message(connection, &response);
}

int forge_message_dispatch(ForgeConnection *connection,
                           const ForgeMessage *message)
{
    if (connection == NULL || message == NULL)
    {
        return -1;
    }

    switch (message->type)
    {
    case FORGE_MESSAGE_PING:
        return forge_dispatch_ping(connection, message);

    case FORGE_MESSAGE_CONNECT:
        return forge_dispatch_connect(connection, message);

    case FORGE_MESSAGE_DISCONNECT:
        return forge_dispatch_disconnect(connection, message);

    default:
        fprintf(stderr, "Unknown Forge message type: %u\n", message->type);

        return forge_dispatch_error(connection, message);
    }
}
