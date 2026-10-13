#ifndef FORGE_METRICS_H
#define FORGE_METRICS_H

#include <stdint.h>

typedef struct
{
    uint64_t started_at_ms;
    uint64_t connections_total;
    uint64_t active_connections;
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
} ForgeMetrics;

void forge_metrics_init(ForgeMetrics *metrics);

void forge_metrics_record_connection_open(ForgeMetrics *metrics);

void forge_metrics_record_connection_close(ForgeMetrics *metrics);

void forge_metrics_record_message_received(ForgeMetrics *metrics);

void forge_metrics_record_message_sent(ForgeMetrics *metrics);

void forge_metrics_record_bytes_received(ForgeMetrics *metrics,
                                         uint64_t byte_count);

void forge_metrics_record_bytes_sent(ForgeMetrics *metrics,
                                     uint64_t byte_count);

uint64_t forge_metrics_get_uptime_ms(const ForgeMetrics *metrics);

void forge_metrics_print(const ForgeMetrics *metrics);

#endif
