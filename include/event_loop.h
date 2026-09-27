#ifndef FORGE_EVENT_LOOP_H
#define FORGE_EVENT_LOOP_H

#include <stdint.h>
#include <sys/epoll.h>

#define FORGE_MAX_EVENTS 1024

typedef struct {
    int fd;
    uint32_t events;
} ForgeEvent;

typedef struct {
    int fd;

    struct epoll_event events[FORGE_MAX_EVENTS];

    int event_count;
} ForgeEventLoop;

int forge_event_loop_init(
    ForgeEventLoop *loop
);

void forge_event_loop_destroy(
    ForgeEventLoop *loop
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

int forge_event_loop_modify(
    ForgeEventLoop *loop,
    int fd,
    uint32_t events
);

int forge_event_loop_wait(
    ForgeEventLoop *loop,
    int timeout
);

int forge_event_loop_get_event(
    ForgeEventLoop *loop,
    int index,
    ForgeEvent *event
);

#endif