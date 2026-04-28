#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <freertos/semphr.h>

class DataAggregator;

class DashboardServer {
public:
    void begin(DataAggregator* aggregator, SemaphoreHandle_t aggregatorMutex);
    void handle();

private:
    void handleRoot();
    void handleStatus();
    void handleSnapshot();
    void handleNotFound();

    WebServer* server_;
    DataAggregator* aggregator_;
    SemaphoreHandle_t aggregatorMutex_;
};