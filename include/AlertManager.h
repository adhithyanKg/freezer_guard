#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class AlertManager {
public:
    AlertManager(const char* url);
    bool sendAlert(const String& eventType, const String& message, float temp = -999.0f);
    int sendAlertGetCode(const String& eventType, const String& message, float temp = -999.0f);

private:
    const char* webhookUrl;
};

#endif