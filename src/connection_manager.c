#include "connection_manager.h"

#include <stdio.h>
#include <stdlib.h>

void forge_connection_manager_init(
    ForgeConnectionManager *manager
)
{
    manager->count = 0;

    for (size_t i = 0; i < FORGE_MAX_CONNECTIONS; i++) {
        manager->connections[i] = NULL;
    }
}

int forge_connection_manager_add(
    ForgeConnectionManager *manager,
    ForgeConnection *connection
)
{
    if (manager->count >= FORGE_MAX_CONNECTIONS) {
        fprintf(stderr, "Connection limit reached.\n");
        return -1;
    }

    for (size_t i = 0; i < FORGE_MAX_CONNECTIONS; i++) {
        if (manager->connections[i] == NULL) {
            manager->connections[i] = connection;
            manager->count++;
            return 0;
        }
    }

    return -1;
}

ForgeConnection *forge_connection_manager_get(
    ForgeConnectionManager *manager,
    int fd
)
{
    for (size_t i = 0; i < FORGE_MAX_CONNECTIONS; i++) {
        ForgeConnection *connection = manager->connections[i];

        if (connection != NULL && connection->fd == fd) {
            return connection;
        }
    }

    return NULL;
}

int forge_connection_manager_remove(
    ForgeConnectionManager *manager,
    int fd
)
{
    for (size_t i = 0; i < FORGE_MAX_CONNECTIONS; i++) {
        ForgeConnection *connection = manager->connections[i];

        if (connection != NULL && connection->fd == fd) {
            forge_connection_close(connection);

            free(connection);

            manager->connections[i] = NULL;
            manager->count--;

            return 0;
        }
    }

    return -1;
}

void forge_connection_manager_destroy(
    ForgeConnectionManager *manager
)
{
    for (size_t i = 0; i < FORGE_MAX_CONNECTIONS; i++) {
        ForgeConnection *connection = manager->connections[i];

        if (connection != NULL) {
            forge_connection_close(connection);
            free(connection);

            manager->connections[i] = NULL;
        }
    }

    manager->count = 0;
}