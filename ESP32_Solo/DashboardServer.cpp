#include "DashboardServer.h"
#include "DataAggregator.h"
#include "index_html.h"

void DashboardServer::begin(DataAggregator* aggregator, SemaphoreHandle_t aggregatorMutex) {
    aggregator_ = aggregator;
    aggregatorMutex_ = aggregatorMutex;
    server_ = new WebServer(80);

    server_->on("/", [this]() { handleRoot(); });
    server_->on("/status", [this]() { handleStatus(); });
    server_->on("/snapshot", [this]() { handleSnapshot(); });
    server_->onNotFound([this]() { handleNotFound(); });

    server_->begin();
    Serial.println("[DashboardServer] HTTP server started on port 80");
}

void DashboardServer::handle() {
    server_->handleClient();
}

void DashboardServer::handleRoot() {
    server_->send(200, "text/html", INDEX_HTML);
}

void DashboardServer::handleStatus() {
    String json;
    if (xSemaphoreTake(aggregatorMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        json = aggregator_->getJson();
        xSemaphoreGive(aggregatorMutex_);
    } else {
        json = "{\"error\":\"mutex timeout\"}";
    }
    server_->send(200, "application/json", json);
}

void DashboardServer::handleSnapshot() {
    // SNAP not implemented — requires bidirectional UART to OpenMV (not wired)
    server_->send(501, "text/plain", "SNAP NOT IMPLEMENTED");
}

void DashboardServer::handleNotFound() {
    server_->send(404, "text/plain", "NOT FOUND");
}