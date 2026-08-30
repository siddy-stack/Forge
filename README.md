# Forge

Forge is a high-performance TCP server framework written in C, built from the ground up to explore systems programming, networking, event-driven I/O, and scalable server architecture.

The project is being developed incrementally, with a focus on clean separation between networking, connections, event handling, and application-level protocols.

## Current Status

Forge currently provides:

- TCP server
- Non-blocking sockets
- Linux `epoll` event loop
- Multiple simultaneous client connections
- Persistent connection management
- Buffered TCP input handling
- Newline-delimited message framing
- Non-blocking output buffering
- `EPOLLIN` / `EPOLLOUT` event handling
- CMake-based builds
- Modular networking architecture

The next major milestone is replacing the temporary newline-based framing with a proper binary protocol layer.

## Architecture

```text
                    Forge Server
                         |
                         v
                  +--------------+
                  |   Event Loop |
                  |    epoll     |
                  +------+-------+
                         |
              +----------+----------+
              |                     |
              v                     v
         EPOLLIN                EPOLLOUT
              |                     |
              v                     v
      +---------------+     +---------------+
      | Client Handler|     | Client Handler|
      +-------+-------+     +-------+-------+
              |                     |
              v                     v
      +---------------+     +---------------+
      |   Connection  |     | Output Buffer |
      |  Input Buffer |     +---------------+
      +-------+-------+
              |
              v
      +---------------+
      | Message Frame |
      |    Parser     |
      +---------------+
```
