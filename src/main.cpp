#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"
#include "TempSensor.h"
#include "DoorSensor.h"
#include "AlertManager.h"

TempSensor tempSensor(TEMP_SENSOR_PIN);
DoorSensor doorSensor(DOOR_SENSOR_PIN);
AlertManager alertManager(WEBHOOK_URL);

unsigned long lastTempCheckTime = 0;
unsigned long lastTempAlertTime = 0;

void setup() {
    Serial.begin(115200);
    tempSensor.begin();
    doorSensor.begin();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[System] Connected to Wi-Fi!");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. DOOR MONITORING (Continuous Check)
    if (doorSensor.isAjarExceeded(DOOR_OPEN_TIMEOUT_MS)) {
        Serial.println("[Alert] Door left open!");
        if (alertManager.sendAlert("DOOR_AJAR", "WARNING: Walk-in door open over 1 minute!")) {
            doorSensor.markAlertTriggered();
        }
    }

    // 2. TEMPERATURE MONITORING (Check every 2 seconds)
    if (currentMillis - lastTempCheckTime >= 2000) {
        lastTempCheckTime = currentMillis;
        float currentTemp = tempSensor.getTemperatureC();

        if (currentTemp != -999.0f) {
            Serial.printf("Temp: %.2f C | Door: %s\n", currentTemp, doorSensor.isOpen() ? "OPEN" : "CLOSED");

            // Evaluate temperature threshold & cooldown timer
            if (currentTemp > MAX_TEMP_THRESHOLD_C) {
                if (currentMillis - lastTempAlertTime >= ALERT_COOLDOWN_MS || lastTempAlertTime == 0) {
                    String msg = "CRITICAL: Freezer temp warm! Current: " + String(currentTemp, 1) + " C";
                    if (alertManager.sendAlert("TEMP_HIGH", msg, currentTemp)) {
                        lastTempAlertTime = currentMillis;
                    }
                }
            }
        } else {
            Serial.println("[Warning] Sensor read error!");
        }
    }
}