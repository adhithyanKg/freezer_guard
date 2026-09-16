#include "AlertManager.h"

AlertManager::AlertManager(const char* url) : webhookUrl(url) {}

bool AlertManager::sendAlert(const String& eventType, const String& message, float temp) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[AlertManager] Error: Wi-Fi not connected!");
        return false;
    }

    HTTPClient http;
    http.begin(webhookUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["event"] = eventType;
    doc["message"] = message;
    if (temp != -999.0f) {
        doc["temperature_c"] = temp;
    }

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int responseCode = http.POST(jsonPayload);
    Serial.printf("[AlertManager] Sent alert. HTTP Code: %d\n", responseCode);
    http.end();

    return (responseCode > 0 && responseCode < 300);
}