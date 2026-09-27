#ifndef FORGE_SERVER_H
#define FORGE_SERVER_H

#include <stdint.h>

enum
{
    FORGE_DEFAULT_PORT = 8080
};

int forge_server_run(uint16_t port);

#endif
