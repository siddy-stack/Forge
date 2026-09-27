#include "server.h"

#include <stdio.h>

int main(void)
{
    printf("Forge server starting...\n");

    return forge_server_run(FORGE_DEFAULT_PORT);
}
