#pragma once

#include <WiFi.h>
#include <WebServer.h>

class DataAggregator;

// ============================================================================
// DashboardServer: WiFi AP + HTTP Dashboard（WebTask 驱动，Core 1）
// DashboardServer: WiFi AP HTTP server serving single-page dashboard
// ============================================================================
class DashboardServer {
public:
    void begin(DataAggregator* aggregator);
    void handle();

private:
    void handleRoot();
    void handleStatus();
    void handleSnapshot();
    void handleNotFound();

    WebServer* server_          = nullptr;
    DataAggregator* aggregator_ = nullptr;
};
