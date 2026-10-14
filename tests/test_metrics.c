#include "metrics.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_connection_metrics(void)
{
    ForgeMetrics metrics;

    forge_metrics_init(&metrics);

    forge_metrics_record_connection_open(&metrics);
    forge_metrics_record_connection_open(&metrics);

    assert(metrics.total_connections == 2U);
    assert(metrics.active_connections == 2U);

    forge_metrics_record_connection_close(&metrics);
    forge_metrics_record_connection_close(&metrics);
    forge_metrics_record_connection_close(&metrics);

    assert(metrics.active_connections == 0U);
    forge_metrics_record_rejected_connection(&metrics);
    forge_metrics_record_protocol_error(&metrics);
    assert(metrics.rejected_connections == 1U);
    assert(metrics.protocol_errors == 1U);
}

static void test_message_and_byte_metrics(void)
{
    ForgeMetrics metrics;

    forge_metrics_init(&metrics);

    forge_metrics_record_message_received(&metrics);
    forge_metrics_record_message_sent(&metrics);
    forge_metrics_record_bytes_received(&metrics, UINT64_C(42));
    forge_metrics_record_bytes_sent(&metrics, UINT64_C(84));

    assert(metrics.messages_received == 1U);
    assert(metrics.messages_sent == 1U);
    assert(metrics.bytes_received == 42U);
    assert(metrics.bytes_sent == 84U);
    (void)forge_metrics_get_uptime_ms(&metrics);
}

int main(void)
{
    test_connection_metrics();
    test_message_and_byte_metrics();

    printf("All metrics tests passed.\n");

    return 0;
}
