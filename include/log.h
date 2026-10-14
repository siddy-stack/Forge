#ifndef FORGE_LOG_H
#define FORGE_LOG_H

#include <stddef.h>
#include <stdint.h>

enum
{
    FORGE_LOG_CAPACITY = 64,
    FORGE_LOG_MESSAGE_SIZE = 160,
    FORGE_LOG_COMPONENT_SIZE = 24
};

typedef enum
{
    FORGE_LOG_DEBUG,
    FORGE_LOG_INFO,
    FORGE_LOG_WARN,
    FORGE_LOG_ERROR
} ForgeLogLevel;

typedef struct
{
    uint64_t uptime_ms;
    ForgeLogLevel level;
    char component[FORGE_LOG_COMPONENT_SIZE];
    char message[FORGE_LOG_MESSAGE_SIZE];
} ForgeLogEntry;

typedef struct
{
    ForgeLogEntry entries[FORGE_LOG_CAPACITY];
    size_t count;
    size_t next_index;
} ForgeLogBook;

void forge_log_init(ForgeLogBook *log_book);

void forge_log_record(ForgeLogBook *log_book, uint64_t uptime_ms,
                      const char *format, ...);

void forge_log_write(ForgeLogBook *log_book, uint64_t uptime_ms,
                     ForgeLogLevel level, const char *component,
                     const char *format, ...);

const char *forge_log_level_name(ForgeLogLevel level);

const ForgeLogEntry *forge_log_get(const ForgeLogBook *log_book, size_t index);

#endif
