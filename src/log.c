#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void forge_log_init(ForgeLogBook *log_book)
{
    if (log_book == NULL)
    {
        return;
    }

    memset(log_book, 0, sizeof(*log_book));
}

void forge_log_record(ForgeLogBook *log_book, uint64_t uptime_ms,
                      const char *format, ...)
{
    if (log_book == NULL || format == NULL)
    {
        return;
    }

    ForgeLogEntry *entry = &log_book->entries[log_book->next_index];
    va_list arguments;

    entry->uptime_ms = uptime_ms;
    entry->level = FORGE_LOG_INFO;
    (void)snprintf(entry->component, sizeof(entry->component), "%s", "server");

    va_start(arguments, format);
    /* NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized) */
    (void)vsnprintf(entry->message, sizeof(entry->message), format, arguments);
    va_end(arguments);

    log_book->next_index = (log_book->next_index + 1U) % FORGE_LOG_CAPACITY;

    if (log_book->count < FORGE_LOG_CAPACITY)
    {
        log_book->count++;
    }
}

void forge_log_write(ForgeLogBook *log_book, uint64_t uptime_ms,
                     ForgeLogLevel level, const char *component,
                     const char *format, ...)
{
    if (log_book == NULL || component == NULL || format == NULL)
    {
        return;
    }

    ForgeLogEntry *entry = &log_book->entries[log_book->next_index];
    va_list arguments;

    entry->uptime_ms = uptime_ms;
    entry->level = level;
    (void)snprintf(entry->component, sizeof(entry->component), "%s", component);
    va_start(arguments, format);
    (void)vsnprintf(entry->message, sizeof(entry->message), format, arguments);
    va_end(arguments);
    log_book->next_index = (log_book->next_index + 1U) % FORGE_LOG_CAPACITY;
    if (log_book->count < FORGE_LOG_CAPACITY)
    {
        log_book->count++;
    }
}

const char *forge_log_level_name(ForgeLogLevel level)
{
    switch (level)
    {
    case FORGE_LOG_DEBUG:
        return "debug";
    case FORGE_LOG_INFO:
        return "info";
    case FORGE_LOG_WARN:
        return "warn";
    case FORGE_LOG_ERROR:
        return "error";
    }

    return "unknown";
}

const ForgeLogEntry *forge_log_get(const ForgeLogBook *log_book, size_t index)
{
    size_t oldest_index;
    size_t entry_index;

    if (log_book == NULL || index >= log_book->count)
    {
        return NULL;
    }

    oldest_index =
        (log_book->next_index + FORGE_LOG_CAPACITY - log_book->count) %
        FORGE_LOG_CAPACITY;
    entry_index = (oldest_index + index) % FORGE_LOG_CAPACITY;

    return &log_book->entries[entry_index];
}
