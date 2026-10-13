#include "connection.h"
#include "connection_manager.h"
#include "dashboard.h"
#include "log.h"
#include "management_api.h"
#include "metrics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void setup(ForgeMetrics *metrics, ForgeConnectionManager *connections,
                  ForgeLogBook *logs)
{
    forge_metrics_init(metrics);
    forge_connection_manager_init(connections);
    forge_log_init(logs);

    forge_metrics_record_connection_open(metrics);
    forge_metrics_record_message_received(metrics);
    forge_metrics_record_message_sent(metrics);
    forge_metrics_record_bytes_received(metrics, 12U);
    forge_metrics_record_bytes_sent(metrics, 34U);
    forge_log_record(logs, 3U, "Client connected: fd=7");
}

static void test_status_and_metrics(void)
{
    ForgeMetrics metrics;
    ForgeConnectionManager connections;
    ForgeLogBook logs;
    char response[FORGE_MANAGEMENT_RESPONSE_SIZE];

    setup(&metrics, &connections, &logs);

    assert(forge_management_api_build_response(response, sizeof(response),
                                               "GET", "/api/status", &metrics,
                                               &connections, &logs) > 0);
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "\"status\":\"ok\"") != NULL);
    assert(strstr(response, "\"active_connections\":0") != NULL);

    assert(forge_management_api_build_response(response, sizeof(response),
                                               "GET", "/api/metrics", &metrics,
                                               &connections, &logs) > 0);
    assert(strstr(response, "\"connections_total\":1") != NULL);
    assert(strstr(response, "\"messages_received\":1") != NULL);
    assert(strstr(response, "\"bytes_sent\":34") != NULL);
}

static void test_connections_and_logs(void)
{
    ForgeMetrics metrics;
    ForgeConnectionManager connections;
    ForgeLogBook logs;
    ForgeConnection connection;
    char response[FORGE_MANAGEMENT_RESPONSE_SIZE];

    setup(&metrics, &connections, &logs);
    forge_connection_init(&connection, 42);
    forge_connection_activate(&connection);
    connection.input_size = 5U;
    connection.output_size = 9U;
    connection.output_offset = 2U;
    assert(forge_connection_manager_add(&connections, &connection) == 0);

    assert(forge_management_api_build_response(
               response, sizeof(response), "GET", "/api/connections", &metrics,
               &connections, &logs) > 0);
    assert(strstr(response, "\"count\":1") != NULL);
    assert(strstr(response, "\"fd\":42") != NULL);
    assert(strstr(response, "\"state\":\"active\"") != NULL);
    assert(strstr(response, "\"pending_output_bytes\":7") != NULL);

    forge_log_record(&logs, 4U, "quoted \"message\"");
    assert(forge_management_api_build_response(response, sizeof(response),
                                               "GET", "/api/logs", &metrics,
                                               &connections, &logs) > 0);
    assert(strstr(response, "\"count\":2") != NULL);
    assert(strstr(response, "quoted \\\"message\\\"") != NULL);
}

static void test_errors(void)
{
    ForgeMetrics metrics;
    ForgeConnectionManager connections;
    ForgeLogBook logs;
    char response[FORGE_MANAGEMENT_RESPONSE_SIZE];

    setup(&metrics, &connections, &logs);

    assert(forge_management_api_build_response(response, sizeof(response),
                                               "POST", "/api/status", &metrics,
                                               &connections, &logs) > 0);
    assert(strstr(response, "HTTP/1.1 405 Method Not Allowed") != NULL);

    assert(forge_management_api_build_response(
               response, sizeof(response), "POST", "/api/control/stop",
               &metrics, &connections, &logs) > 0);
    assert(strstr(response, "\"status\":\"stopping\"") != NULL);

    assert(forge_management_api_build_response(response, sizeof(response),
                                               "GET", "/api/unknown", &metrics,
                                               &connections, &logs) > 0);
    assert(strstr(response, "HTTP/1.1 404 Not Found") != NULL);
}

static void test_dashboard_page(void)
{
    const char *dashboard = forge_dashboard_html();

    assert(dashboard != NULL);
    assert(forge_dashboard_html_size() > 0U);
    assert(strstr(dashboard, "Forge Control Room") != NULL);
    assert(strstr(dashboard, "/api/metrics") != NULL);
    assert(strstr(dashboard, "Server Controls") != NULL);
}

int main(void)
{
    test_status_and_metrics();
    test_connections_and_logs();
    test_errors();
    test_dashboard_page();

    printf("All management API tests passed.\n");

    return 0;
}
