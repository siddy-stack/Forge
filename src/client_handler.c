#include "client_handler.h"

#include <stdio.h>
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

    /*
     * Process every complete message currently
     * available in the input buffer.
     */
    for (;;) {
        ForgeMessage message;
        size_t bytes_consumed = 0;

        int message_result =
            forge_connection_get_message(
                connection,
                &message,
                &bytes_consumed
            );

        /*
         * Not enough bytes for a complete message.
         * Keep the data in the input buffer until
         * the next EPOLLIN event.
         */
        if (message_result == 0) {
            break;
        }

        /*
         * Invalid protocol data.
         */
        if (message_result == -1) {
            fprintf(
                stderr,
                "Invalid Forge protocol message "
                "from fd=%d\n",
                fd
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
            "Received Forge message: "
            "fd=%d type=%u request_id=%u "
            "payload=%u bytes\n",
            fd,
            message.type,
            message.request_id,
            message.payload_length
        );

        /*
         * Echo the complete Forge message back
         * to the client.
         */
        if (forge_connection_queue_message(
                connection,
                &message
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

        /*
         * Only consume the message after the response
         * has been successfully queued.
         */
        if (forge_connection_consume_message(
                connection,
                bytes_consumed
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