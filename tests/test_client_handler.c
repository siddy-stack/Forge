#include "client_handler.h"
#include "protocol.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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

static ForgeEvent wait_for_event(ForgeEventLoop *event_loop)
{
    ForgeEvent event;

    assert(forge_event_loop_wait(event_loop, 0) == 1);
    assert(forge_event_loop_get_event(event_loop, 0, &event) == 0);

    return event;
}

static void destroy_fixture(ForgeEventLoop *event_loop,
                            ForgeConnectionManager *connection_manager,
                            int file_descriptor, int peer_file_descriptor)
{
    assert(forge_event_loop_remove(event_loop, file_descriptor) == 0);
    assert(forge_connection_manager_remove(connection_manager,
                                           file_descriptor) == 0);

    forge_event_loop_destroy(event_loop);
    close(peer_file_descriptor);
}

static void test_read_enables_write_events(void)
{
    ForgeEventLoop event_loop;
    ForgeConnectionManager connection_manager;
    ForgeMessage message;
    ForgeConnection *connection;
    int sockets[2];
    uint8_t buffer[FORGE_CONNECTION_BUFFER_SIZE];
    const uint8_t payload[] = "ping";
    int encoded_size;
    ForgeEvent event;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    assert(forge_event_loop_init(&event_loop) == 0);

    forge_connection_manager_init(&connection_manager);
    connection = create_connection(sockets[0]);

    assert(forge_connection_manager_add(&connection_manager, connection) == 0);
    assert(forge_event_loop_add(&event_loop, sockets[0], EPOLLIN) == 0);

    message = (ForgeMessage){.type = FORGE_MESSAGE_PING,
                             .flags = 0,
                             .request_id = 1,
                             .payload = payload,
                             .payload_length = sizeof(payload) - 1};

    encoded_size = forge_protocol_encode(&message, buffer, sizeof(buffer));

    assert(encoded_size > 0);
    assert(send(sockets[1], buffer, (size_t)encoded_size, 0) == encoded_size);
    assert(forge_client_handle_read(&event_loop, &connection_manager,
                                    connection) == 1);

    event = wait_for_event(&event_loop);

    assert(event.fd == sockets[0]);
    assert((event.events & EPOLLOUT) != 0U);

    assert(forge_client_handle_write(&event_loop, &connection_manager,
                                     connection) == 1);
    assert(forge_event_loop_wait(&event_loop, 0) == 0);

    destroy_fixture(&event_loop, &connection_manager, sockets[0], sockets[1]);
}

static void test_write_disables_write_events(void)
{
    ForgeEventLoop event_loop;
    ForgeConnectionManager connection_manager;
    ForgeConnection *connection;
    int sockets[2];
    char byte;
    ForgeEvent event;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    assert(forge_event_loop_init(&event_loop) == 0);

    forge_connection_manager_init(&connection_manager);
    connection = create_connection(sockets[0]);

    assert(forge_connection_manager_add(&connection_manager, connection) == 0);
    assert(forge_event_loop_add(&event_loop, sockets[0], EPOLLIN) == 0);
    assert(forge_connection_queue_send(connection, "x", 1) == 0);
    assert(forge_client_handle_write(&event_loop, &connection_manager,
                                     connection) == 1);
    assert(recv(sockets[1], &byte, sizeof(byte), 0) == 1);
    assert(byte == 'x');

    assert(send(sockets[1], "y", 1, 0) == 1);

    event = wait_for_event(&event_loop);

    assert(event.fd == sockets[0]);
    assert((event.events & EPOLLIN) != 0U);
    assert((event.events & EPOLLOUT) == 0U);

    destroy_fixture(&event_loop, &connection_manager, sockets[0], sockets[1]);
}

int main(void)
{
    test_read_enables_write_events();
    test_write_disables_write_events();

    printf("All client handler tests passed.\n");

    return 0;
}
