#ifndef FORGE_CONNECTION_MANAGER_H
#define FORGE_CONNECTION_MANAGER_H

#include "connection.h"

#include <stddef.h>

#define FORGE_MAX_CONNECTIONS 1024

typedef struct {
    ForgeConnection *connections[FORGE_MAX_CONNECTIONS];
    size_t count;
} ForgeConnectionManager;

void forge_connection_manager_init(
    ForgeConnectionManager *manager
);

int forge_connection_manager_add(
    ForgeConnectionManager *manager,
    ForgeConnection *connection
);

ForgeConnection *forge_connection_manager_get(
    ForgeConnectionManager *manager,
    int fd
);

int forge_connection_manager_remove(
    ForgeConnectionManager *manager,
    int fd
);

void forge_connection_manager_destroy(
    ForgeConnectionManager *manager
);

#endif