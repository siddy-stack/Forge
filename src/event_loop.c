#include "event_loop.h"

#include <errno.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

int forge_event_loop_init(ForgeEventLoop *loop)
{
    loop->fd = epoll_create1(0);
    loop->event_count = 0;

    if (loop->fd == -1) {
        perror("Failed to create epoll instance");
        return -1;
    }

    return 0;
}

int forge_event_loop_add(
    ForgeEventLoop *loop,
    int fd,
    uint32_t events
)
{
    struct epoll_event event = {
        .events = events,
        .data.fd = fd,
    };

    if (epoll_ctl(loop->fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("Failed to add file descriptor to epoll");
        return -1;
    }

    return 0;
}

int forge_event_loop_modify(
    ForgeEventLoop *loop,
    int fd,
    uint32_t events
)
{
    struct epoll_event event = {
        .events = events,
        .data.fd = fd,
    };

    if (epoll_ctl(
            loop->fd,
            EPOLL_CTL_MOD,
            fd,
            &event
        ) == -1) {
        perror(
            "Failed to modify file descriptor in epoll"
        );

        return -1;
    }

    return 0;
}

int forge_event_loop_remove(
    ForgeEventLoop *loop,
    int fd
)
{
    if (epoll_ctl(loop->fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        perror("Failed to remove file descriptor from epoll");
        return -1;
    }

    return 0;
}

int forge_event_loop_wait(
    ForgeEventLoop *loop,
    int timeout_ms
)
{
    int event_count = epoll_wait(
        loop->fd,
        loop->events,
        FORGE_MAX_EVENTS,
        timeout_ms
    );

    if (event_count == -1) {
        if (errno == EINTR) {
            loop->event_count = 0;
            return 0;
        }

        perror("epoll_wait failed");
        return -1;
    }

    loop->event_count = event_count;

    return event_count;
}

int forge_event_loop_get_fd(
    const ForgeEventLoop *loop,
    int index
)
{
    if (index < 0 || index >= loop->event_count) {
        return -1;
    }

    return loop->events[index].data.fd;
}

uint32_t forge_event_loop_get_events(
    const ForgeEventLoop *loop,
    int index
)
{
    if (index < 0 || index >= loop->event_count) {
        return 0;
    }

    return loop->events[index].events;
}

void forge_event_loop_destroy(ForgeEventLoop *loop)
{
    if (loop->fd >= 0) {
        close(loop->fd);
        loop->fd = -1;
    }

    loop->event_count = 0;
}