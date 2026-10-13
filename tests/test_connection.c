#include "connection.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void test_initial_state(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);

    assert(forge_connection_get_state(&connection) ==
           FORGE_CONNECTION_ACCEPTED);
}

static void test_activation(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);
    forge_connection_activate(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_ACTIVE);

    forge_connection_activate(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_ACTIVE);
}

static void test_begin_close(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);
    forge_connection_activate(&connection);
    forge_connection_begin_close(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_CLOSING);

    forge_connection_begin_close(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_CLOSING);
}

static void test_close(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);
    forge_connection_activate(&connection);
    forge_connection_begin_close(&connection);
    forge_connection_close(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_CLOSED);
    assert(connection.fd == -1);
    assert(connection.input_size == 0);
    assert(connection.output_size == 0);
    assert(connection.output_offset == 0);

    forge_connection_close(&connection);

    assert(forge_connection_get_state(&connection) == FORGE_CONNECTION_CLOSED);
}

static void test_null_state(void)
{
    assert(forge_connection_get_state(NULL) == FORGE_CONNECTION_CLOSED);

    forge_connection_init(NULL, -1);
    forge_connection_activate(NULL);
    forge_connection_begin_close(NULL);
    forge_connection_close(NULL);
}

static void test_fragmented_receive(void)
{
    ForgeConnection connection;
    int sockets[2];
    const char first_chunk[] = "abc";
    const char second_chunk[] = "def";

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    forge_connection_init(&connection, sockets[0]);
    forge_connection_activate(&connection);

    assert(send(sockets[1], first_chunk, sizeof(first_chunk) - 1, 0) ==
           (ssize_t)(sizeof(first_chunk) - 1));
    assert(forge_connection_receive(&connection) == 1);
    assert(connection.input_size == sizeof(first_chunk) - 1);

    assert(send(sockets[1], second_chunk, sizeof(second_chunk) - 1, 0) ==
           (ssize_t)(sizeof(second_chunk) - 1));
    assert(forge_connection_receive(&connection) == 1);
    assert(connection.input_size ==
           sizeof(first_chunk) + sizeof(second_chunk) - 2);
    assert(memcmp(connection.input_buffer, "abcdef", connection.input_size) ==
           0);

    forge_connection_close(&connection);
    close(sockets[1]);
}

static void test_output_compaction(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);
    assert(forge_connection_queue_send(&connection, "abcdef", 6) == 0);

    connection.output_offset = 3;

    assert(forge_connection_queue_send(&connection, "gh", 2) == 0);
    assert(connection.output_offset == 0);
    assert(connection.output_size == 5);
    assert(memcmp(connection.output_buffer, "defgh", connection.output_size) ==
           0);
}

static void test_flush(void)
{
    ForgeConnection connection;
    int sockets[2];
    char buffer[4];

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    forge_connection_init(&connection, sockets[0]);
    forge_connection_activate(&connection);

    assert(forge_connection_queue_send(&connection, "forge", 5) == 0);
    assert(forge_connection_flush(&connection) == 0);
    assert(!forge_connection_has_pending_output(&connection));
    assert(recv(sockets[1], buffer, sizeof(buffer), 0) == 4);
    assert(memcmp(buffer, "forg", sizeof(buffer)) == 0);
    assert(recv(sockets[1], buffer, 1, 0) == 1);
    assert(buffer[0] == 'e');

    forge_connection_close(&connection);
    close(sockets[1]);
}

static void test_idle_timeout(void)
{
    ForgeConnection connection;

    forge_connection_init(&connection, -1);
    forge_connection_activate(&connection);

    connection.last_activity_ms = 1000;

    assert(!forge_connection_is_idle(&connection, 1599, 600));
    assert(forge_connection_is_idle(&connection, 1600, 600));
    assert(!forge_connection_is_idle(&connection, 999, 600));

    forge_connection_begin_close(&connection);

    assert(!forge_connection_is_idle(&connection, 2000, 600));
}

int main(void)
{
    test_initial_state();
    test_activation();
    test_begin_close();
    test_close();
    test_null_state();
    test_fragmented_receive();
    test_output_compaction();
    test_flush();
    test_idle_timeout();

    printf("All connection state tests passed.\n");

    return 0;
}
