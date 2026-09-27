#ifndef FORGE_MESSAGE_DISPATCHER_H
#define FORGE_MESSAGE_DISPATCHER_H

#include "connection.h"

int forge_message_dispatch(ForgeConnection *connection,
                           const ForgeMessage *message);

#endif
