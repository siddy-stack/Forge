#ifndef FORGE_MANAGEMENT_API_H
#define FORGE_MANAGEMENT_API_H

#include "connection_manager.h"
#include "event_loop.h"
#include "log.h"
#include "metrics.h"

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    FORGE_DEFAULT_MANAGEMENT_PORT = 8081,
    FORGE_MANAGEMENT_MAX_CLIENTS = 8,
    FORGE_MANAGEMENT_REQUEST_SIZE = 4096,
    FORGE_MANAGEMENT_RESPONSE_SIZE = 131072
};

typedef struct
{
    int fd;
    char request[FORGE_MANAGEMENT_REQUEST_SIZE];
    size_t request_size;
    char response[FORGE_MANAGEMENT_RESPONSE_SIZE];
    size_t response_size;
    size_t response_offset;
    int shutdown_after_response;
} ForgeManagementClient;

typedef struct
{
    int listener_fd;
    uint16_t port;
    ForgeManagementClient clients[FORGE_MANAGEMENT_MAX_CLIENTS];
} ForgeManagementApi;

void forge_management_api_init(ForgeManagementApi *api);

int forge_management_api_start(ForgeManagementApi *api,
                               ForgeEventLoop *event_loop, uint16_t port);

void forge_management_api_stop(ForgeManagementApi *api,
                               ForgeEventLoop *event_loop);

int forge_management_api_handles_fd(const ForgeManagementApi *api,
                                    int file_descriptor);

int forge_management_api_handle_event(
    ForgeManagementApi *api, ForgeEventLoop *event_loop,
    const ForgeEvent *event, const ForgeMetrics *metrics,
    const ForgeConnectionManager *connections, const ForgeLogBook *logs,
    volatile sig_atomic_t *shutdown_requested);

int forge_management_api_build_response(
    char *response, size_t response_size, const char *method, const char *path,
    const ForgeMetrics *metrics, const ForgeConnectionManager *connections,
    const ForgeLogBook *logs);

#endif
