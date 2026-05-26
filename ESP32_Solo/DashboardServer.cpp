#include "DashboardServer.h"
#include "DataAggregator.h"
#include "index_html.h"

void DashboardServer::begin(DataAggregator* aggregator) {
    aggregator_ = aggregator;
    server_ = new WebServer(80);

    server_->on("/", [this]() { handleRoot(); });
    server_->on("/status", [this]() { handleStatus(); });
    server_->onNotFound([this]() { handleNotFound(); });

    server_->begin();
    Serial.println("[Dashboard] HTTP :80");
}

void DashboardServer::handle() {
    server_->handleClient();
}

void DashboardServer::handleRoot() {
    server_->send(200, "text/html", INDEX_HTML);
}

void DashboardServer::handleStatus() {
    String json;
    if (aggregator_->lock(pdMS_TO_TICKS(100))) {
        json = aggregator_->getJson();
        aggregator_->unlock();
    } else {
        json = "{\"error\":\"mutex timeout\"}";
    }

    // 注入系统信息
    char sysBuf[64];
    snprintf(sysBuf, sizeof(sysBuf),
        ",\"sys\":{\"uptime\":%lu,\"wifi_clients\":%d}",
        millis(),
        WiFi.softAPgetStationNum());
    // 在末尾 '}' 前插入 sys
    int lastBrace = json.lastIndexOf('}');
    if (lastBrace > 0) {
        json = json.substring(0, lastBrace) + sysBuf + "}";
    }

    server_->send(200, "application/json", json);
}

// /snapshot: 未实现 (需要双向 UART 到 OpenMV，未接线)
void DashboardServer::handleSnapshot() {
    server_->send(501, "text/plain", "SNAP NOT IMPLEMENTED");
}

void DashboardServer::handleNotFound() {
    server_->send(404, "text/plain", "NOT FOUND");
}
