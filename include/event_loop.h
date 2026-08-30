#ifndef FORGE_EVENT_LOOP_H
#define FORGE_EVENT_LOOP_H

#include <stdint.h>
#include <sys/epoll.h>

#define FORGE_MAX_EVENTS 64

typedef struct {
    int fd;
    struct epoll_event events[FORGE_MAX_EVENTS];
    int event_count;
} ForgeEventLoop;

int forge_event_loop_init(ForgeEventLoop *loop);

int forge_event_loop_modify(
    ForgeEventLoop *loop,
    int fd,
    uint32_t events
);

int forge_event_loop_add(
    ForgeEventLoop *loop,
    int fd,
    uint32_t events
);

int forge_event_loop_remove(
    ForgeEventLoop *loop,
    int fd
);

int forge_event_loop_wait(
    ForgeEventLoop *loop,
    int timeout_ms
);

int forge_event_loop_get_fd(
    const ForgeEventLoop *loop,
    int index
);

uint32_t forge_event_loop_get_events(
    const ForgeEventLoop *loop,
    int index
);

void forge_event_loop_destroy(ForgeEventLoop *loop);

#endif