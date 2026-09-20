#include "AlertManager.h"

AlertManager::AlertManager(const char* url) : webhookUrl(url) {}

int AlertManager::sendAlertGetCode(const String& eventType, const String& message, float temp) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[AlertManager] Error: Wi-Fi not connected!");
        return -1;
    }

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(webhookUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["event"] = eventType;
    doc["message"] = message;

    // Sanitize float to prevent invalid JSON (NaN / Inf)
    if (temp != -999.0f && !isnan(temp) && !isinf(temp)) {
        doc["temperature_c"] = roundf(temp * 100.0f) / 100.0f;
    }

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int responseCode = http.POST(jsonPayload);
    Serial.printf("[AlertManager] Sent alert. HTTP Code: %d\n", responseCode);
    http.end();

    return responseCode;
}

bool AlertManager::sendAlert(const String& eventType, const String& message, float temp) {
    int code = sendAlertGetCode(eventType, message, temp);
    return (code >= 200 && code < 300);
}