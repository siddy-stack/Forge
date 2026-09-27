#ifndef FORGE_NETWORK_H
#define FORGE_NETWORK_H

#include <stdint.h>

enum
{
    FORGE_BACKLOG = 16
};

int forge_network_create_listener(uint16_t port);

int forge_network_accept_client(int server_file_descriptor);

int forge_network_set_nonblocking(int file_descriptor);

#endif
