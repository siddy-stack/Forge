#include "connection_manager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static ForgeConnection *create_connection(int file_descriptor)
{
    ForgeConnection *connection = malloc(sizeof(*connection));

    assert(connection != NULL);

    forge_connection_init(connection, file_descriptor);
    forge_connection_activate(connection);

    return connection;
}

static void test_remove_closes_connection(void)
{
    ForgeConnectionManager manager;
    int sockets[2];
    char byte;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    forge_connection_manager_init(&manager);

    ForgeConnection *connection = create_connection(sockets[0]);

    assert(forge_connection_manager_add(&manager, connection) == 0);
    assert(manager.count == 1);
    assert(forge_connection_manager_get(&manager, sockets[0]) == connection);

    assert(forge_connection_manager_remove(&manager, sockets[0]) == 0);
    assert(manager.count == 0);
    assert(forge_connection_manager_get(&manager, sockets[0]) == NULL);
    assert(read(sockets[1], &byte, sizeof(byte)) == 0);
    assert(forge_connection_manager_remove(&manager, sockets[0]) == -1);

    close(sockets[1]);
}

static void test_destroy_closes_connections(void)
{
    ForgeConnectionManager manager;
    int sockets[2];
    char byte;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    forge_connection_manager_init(&manager);
    assert(forge_connection_manager_add(&manager,
                                        create_connection(sockets[0])) == 0);

    forge_connection_manager_destroy(&manager);

    assert(manager.count == 0);
    assert(read(sockets[1], &byte, sizeof(byte)) == 0);

    close(sockets[1]);
}

static void test_null_arguments(void)
{
    assert(forge_connection_manager_add(NULL, NULL) == -1);
    assert(forge_connection_manager_get(NULL, -1) == NULL);
    assert(forge_connection_manager_remove(NULL, -1) == -1);

    forge_connection_manager_init(NULL);
    forge_connection_manager_destroy(NULL);
}

static void test_connection_limit(void)
{
    ForgeConnectionManager manager;
    ForgeConnection *extra_connection;

    forge_connection_manager_init(&manager);

    for (size_t index = 0; index < FORGE_MAX_CONNECTIONS; index++)
    {
        assert(forge_connection_manager_add(&manager, create_connection(-1)) ==
               0);
    }

    extra_connection = create_connection(-1);

    assert(forge_connection_manager_add(&manager, extra_connection) == -1);
    assert(manager.count == FORGE_MAX_CONNECTIONS);

    forge_connection_close(extra_connection);
    free(extra_connection);

    forge_connection_manager_destroy(&manager);
}

int main(void)
{
    test_remove_closes_connection();
    test_destroy_closes_connections();
    test_null_arguments();
    test_connection_limit();

    printf("All connection manager tests passed.\n");

    return 0;
}
