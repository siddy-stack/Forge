#include "client_handler.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

int forge_client_handle_read(
    ForgeEventLoop *event_loop,
    ForgeConnectionManager *connection_manager,
    ForgeConnection *connection
)
{
    int fd = connection->fd;

    int result = forge_connection_receive(
        connection
    );

    if (result == 0) {
        forge_event_loop_remove(
            event_loop,
            fd
        );

        forge_connection_manager_remove(
            connection_manager,
            fd
        );

        printf(
            "Client disconnected: fd=%d\n",
            fd
        );

        return 0;
    }

    if (result == -1) {
        forge_event_loop_remove(
            event_loop,
            fd
        );

        forge_connection_manager_remove(
            connection_manager,
            fd
        );

        return -1;
    }

    char message[FORGE_CONNECTION_BUFFER_SIZE];

    for (;;) {
        int message_result =
            forge_connection_get_message(
                connection,
                message,
                sizeof(message)
            );

        if (message_result == 0) {
            break;
        }

        if (message_result == -1) {
            fprintf(
                stderr,
                "Failed to extract message "
                "from connection.\n"
            );

            forge_event_loop_remove(
                event_loop,
                fd
            );

            forge_connection_manager_remove(
                connection_manager,
                fd
            );

            return -1;
        }

        printf(
            "Received from fd=%d: %s",
            fd,
            message
        );

        if (forge_connection_queue_send(
                connection,
                message,
                strlen(message)
            ) == -1) {
            forge_event_loop_remove(
                event_loop,
                fd
            );

            forge_connection_manager_remove(
                connection_manager,
                fd
            );

            return -1;
        }
    }

    if (forge_connection_has_pending_output(
            connection
        )) {
        if (forge_event_loop_modify(
                event_loop,
                fd,
                EPOLLIN | EPOLLOUT
            ) == -1) {
            forge_event_loop_remove(
                event_loop,
                fd
            );

            forge_connection_manager_remove(
                connection_manager,
                fd
            );

            return -1;
        }
    }

    return 1;
}

int forge_client_handle_write(
    ForgeEventLoop *event_loop,
    ForgeConnectionManager *connection_manager,
    ForgeConnection *connection
)
{
    int fd = connection->fd;

    int result = forge_connection_flush(
        connection
    );

    if (result == -1) {
        forge_event_loop_remove(
            event_loop,
            fd
        );

        forge_connection_manager_remove(
            connection_manager,
            fd
        );

        return -1;
    }

    if (!forge_connection_has_pending_output(
            connection
        )) {
        if (forge_event_loop_modify(
                event_loop,
                fd,
                EPOLLIN
            ) == -1) {
            forge_event_loop_remove(
                event_loop,
                fd
            );

            forge_connection_manager_remove(
                connection_manager,
                fd
            );

            return -1;
        }
    }

    return 1;
} 