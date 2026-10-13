#include "metrics.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static uint64_t forge_metrics_get_time_ms(void)
{
    struct timespec time;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        return 0;
    }

    return (uint64_t)time.tv_sec * 1000U + (uint64_t)time.tv_nsec / 1000000U;
}

void forge_metrics_init(ForgeMetrics *metrics)
{
    if (metrics == NULL)
    {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));

    metrics->started_at_ms = forge_metrics_get_time_ms();
}

void forge_metrics_record_connection_open(ForgeMetrics *metrics)
{
    if (metrics == NULL)
    {
        return;
    }

    metrics->connections_total++;
    metrics->active_connections++;
}

void forge_metrics_record_connection_close(ForgeMetrics *metrics)
{
    if (metrics == NULL || metrics->active_connections == 0)
    {
        return;
    }

    metrics->active_connections--;
}

void forge_metrics_record_message_received(ForgeMetrics *metrics)
{
    if (metrics == NULL)
    {
        return;
    }

    metrics->messages_received++;
}

void forge_metrics_record_message_sent(ForgeMetrics *metrics)
{
    if (metrics == NULL)
    {
        return;
    }

    metrics->messages_sent++;
}

void forge_metrics_record_bytes_received(ForgeMetrics *metrics,
                                         uint64_t byte_count)
{
    if (metrics == NULL)
    {
        return;
    }

    metrics->bytes_received += byte_count;
}

void forge_metrics_record_bytes_sent(ForgeMetrics *metrics, uint64_t byte_count)
{
    if (metrics == NULL)
    {
        return;
    }

    metrics->bytes_sent += byte_count;
}

uint64_t forge_metrics_get_uptime_ms(const ForgeMetrics *metrics)
{
    uint64_t now_ms;

    if (metrics == NULL)
    {
        return 0;
    }

    now_ms = forge_metrics_get_time_ms();

    if (now_ms < metrics->started_at_ms)
    {
        return 0;
    }

    return now_ms - metrics->started_at_ms;
}

void forge_metrics_print(const ForgeMetrics *metrics)
{
    if (metrics == NULL)
    {
        return;
    }

    printf("Forge metrics: uptime_ms=%" PRIu64 " connections_total=%" PRIu64
           " active_connections=%" PRIu64 " messages_received=%" PRIu64
           " messages_sent=%" PRIu64 " bytes_received=%" PRIu64
           " bytes_sent=%" PRIu64 "\n",
           forge_metrics_get_uptime_ms(metrics), metrics->connections_total,
           metrics->active_connections, metrics->messages_received,
           metrics->messages_sent, metrics->bytes_received,
           metrics->bytes_sent);
}
