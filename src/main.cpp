#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"
#include "TempSensor.h"
#include "DoorSensor.h"
#include "AlertManager.h"
#include "StorageManager.h"

TempSensor tempSensor(TEMP_SENSOR_PIN);
DoorSensor doorSensor(DOOR_SENSOR_PIN);
AlertManager alertManager(WEBHOOK_URL);
StorageManager storageManager;

// Timers
unsigned long lastTempCheckTime = 0;
unsigned long lastTempAlertTime = 0;
unsigned long last30MinLogTime = 0;
unsigned long lastSyncCheckTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    tempSensor.begin();
    doorSensor.begin();
    storageManager.begin();
    // storageManager.clearBuffer();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to Wi-Fi");
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {
        delay(WIFI_RETRY_INTERVAL_MS);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[System] Connected to Wi-Fi!");
    } else {
        Serial.println("\n[Warning] Initial Wi-Fi connection timed out. Working Offline.");
    }
}

void loop() {
    unsigned long currentMillis = millis();

    // =========================================================================
    // RULE 1 & 2: CONTINUOUS SENSOR MONITORING & ALERTS
    // =========================================================================
    
    // Check Sensors every 2 seconds
    if (currentMillis - lastTempCheckTime >= SENSOR_CHECK_INTERVAL_MS) {
        lastTempCheckTime = currentMillis;
        float currentTemp = tempSensor.getTemperatureC();
        bool doorOpen = doorSensor.isOpen();

        // Print active state to Serial Monitor for viewing
        Serial.printf("[Monitor] Temp: %.2f C | Door: %s\n", currentTemp, doorOpen ? "OPEN" : "CLOSED");

        // --- RULE 1: Temp Alert (> -5 C) ---
        if (currentTemp != INVALID_TEMPERATURE_C && currentTemp > MAX_TEMP_THRESHOLD_C) {
            if (currentMillis - lastTempAlertTime >= ALERT_COOLDOWN_MS || lastTempAlertTime == 0) {
                lastTempAlertTime = currentMillis;

                if (WiFi.status() == WL_CONNECTED) {
                    String msg = "ALERT: Temp exceeded threshold! Current: " + String(currentTemp, TEMPERATURE_DECIMAL_PLACES) + " C";
                    Serial.println("[Alert] Wi-Fi UP -> Sending Temp Alert to Webhook...");
                    alertManager.sendAlert("TEMP_HIGH_ALERT", msg, currentTemp);
                } else {
                    Serial.println("[Alert] Wi-Fi DOWN -> Printing Temp Alert to Serial (Not stored in buffer)");
                }
            }
        }
    }

    // --- RULE 2: Door Alert (Open > 15 Mins) ---
    if (doorSensor.isAjarExceeded(DOOR_OPEN_TIMEOUT_MS)) {
        doorSensor.markAlertTriggered(); // Prevents repeated firing while door stays open

        if (WiFi.status() == WL_CONNECTED) {
            float currentTemp = tempSensor.getTemperatureC();
            String msg = "ALERT: Door has been left open for over 15 minutes!";
            Serial.printf("[Alert] Wi-Fi UP -> Sending Door Alert to Webhook (Temp: %.2f C)...\n", currentTemp);
            alertManager.sendAlert("DOOR_OPEN_ALERT", msg, currentTemp);
        } else {
            Serial.println("[Alert] Wi-Fi DOWN -> Printing Door Alert to Serial (Not stored in buffer)");
        }
    }

    // =========================================================================
    // RULE 3: 30-MINUTE PERIODIC TELEMETRY (SNAPSHOT & SEND / BUFFER)
    // =========================================================================
    if (currentMillis - last30MinLogTime >= TELEMETRY_INTERVAL_MS || last30MinLogTime == 0) {
        last30MinLogTime = currentMillis;
        
        float currentTemp = tempSensor.getTemperatureC();
        bool doorStatus = doorSensor.isOpen();

        Serial.println("\n--- [30-Min Telemetry Routine] ---");
        Serial.printf("[Telemetry] Temp: %.2f C | Door: %s\n", currentTemp, doorStatus ? "OPEN" : "CLOSED");

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[Telemetry] Wi-Fi UP -> Sending 30-min log to Webhook...");
            String msg = "PERIODIC_LOG: Temp=" + String(currentTemp, TEMPERATURE_DECIMAL_PLACES) + "C, Door=" + String(doorStatus ? "OPEN" : "CLOSED");
            alertManager.sendAlert("PERIODIC_TELEMETRY", msg, currentTemp);
        } else {
            Serial.println("[Telemetry] Wi-Fi DOWN -> Saving 30-min log to LittleFS internal memory...");
            storageManager.saveReading(currentTemp, doorStatus);
        }
        Serial.println("-----------------------------------\n");
    }

    // =========================================================================
    // RULE 4: FLUSH INTERNAL STORAGE WHEN WI-FI IS RECONNECTED
    // =========================================================================
    if (currentMillis - lastSyncCheckTime >= STORAGE_SYNC_INTERVAL_MS) {
        lastSyncCheckTime = currentMillis;

        if (WiFi.status() == WL_CONNECTED) {
            size_t bufferedCount = storageManager.getBufferedCount();
            if (bufferedCount == 0) {
                Serial.println("[Sync] Wi-Fi connected; no offline logs to flush.");
                return;
            }

            Serial.printf("[Sync] Found %u offline 30-min logs in internal memory. Flushing...\n", (unsigned)bufferedCount);

            DataPacket packet;
            if (storageManager.getOldestReading(packet)) {
                String syncMsg = "STORED_OFFLINE_LOG: Temp=" + String(packet.temperature, TEMPERATURE_DECIMAL_PLACES) + "C";
                int httpCode = alertManager.sendAlertGetCode("SYNC_TELEMETRY", syncMsg, packet.temperature);
                
                if (httpCode >= 200 && httpCode < 300) {
                    storageManager.popOldestReading();
                    Serial.println("[Sync] Packet pushed to Webhook successfully.");
                } else if (httpCode >= 400 && httpCode < 500) {
                    Serial.printf("[Sync] Bad request (HTTP %d). Removing bad packet.\n", httpCode);
                    storageManager.popOldestReading();
                } else {
                    Serial.printf("[Sync] Webhook unreachable (HTTP %d). Will retry next cycle.\n", httpCode);
                }
            }
        }
    }
}