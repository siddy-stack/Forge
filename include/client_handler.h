#ifndef FORGE_CLIENT_HANDLER_H
#define FORGE_CLIENT_HANDLER_H

#include "connection.h"
#include "connection_manager.h"
#include "event_loop.h"

int forge_client_handle_read(
    ForgeEventLoop *event_loop,
    ForgeConnectionManager *connection_manager,
    ForgeConnection *connection
);

int forge_client_handle_write(
    ForgeEventLoop *event_loop,
    ForgeConnectionManager *connection_manager,
    ForgeConnection *connection
);

#endif