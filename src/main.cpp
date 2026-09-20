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

// Configuration Thresholds
const float TEMP_ALERT_THRESHOLD = 20.0f;                      // Alert if temp < 20 C
const unsigned long DOOR_ALERT_TIMEOUT_MS = 15 * 60 * 1000;    // 15 Minutes
const unsigned long TELEMETRY_INTERVAL_MS = 30 * 60 * 1000;    // 30 Minutes
const unsigned long TEMP_ALERT_COOLDOWN = 60 * 1000;          // 1 minute alert cooldown

void setup() {
    Serial.begin(115200);
    tempSensor.begin();
    doorSensor.begin();
    storageManager.begin();
    // storageManager.clearBuffer();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to Wi-Fi");
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
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
    if (currentMillis - lastTempCheckTime >= 2000) {
        lastTempCheckTime = currentMillis;
        float currentTemp = tempSensor.getTemperatureC();
        bool doorOpen = doorSensor.isOpen();

        // Print active state to Serial Monitor for viewing
        Serial.printf("[Monitor] Temp: %.2f C | Door: %s\n", currentTemp, doorOpen ? "OPEN" : "CLOSED");

        // --- RULE 1: Temp Alert (< 20 C) ---
        if (currentTemp != -999.0f && currentTemp < TEMP_ALERT_THRESHOLD) {
            if (currentMillis - lastTempAlertTime >= TEMP_ALERT_COOLDOWN || lastTempAlertTime == 0) {
                lastTempAlertTime = currentMillis;

                if (WiFi.status() == WL_CONNECTED) {
                    String msg = "ALERT: Temp dropped below 20 C! Current: " + String(currentTemp, 1) + " C";
                    Serial.println("[Alert] Wi-Fi UP -> Sending Temp Alert to Webhook...");
                    alertManager.sendAlert("TEMP_LOW_ALERT", msg, currentTemp);
                } else {
                    Serial.println("[Alert] Wi-Fi DOWN -> Printing Temp Alert to Serial (Not stored in buffer)");
                }
            }
        }
    }

    // --- RULE 2: Door Alert (Open > 15 Mins) ---
    if (doorSensor.isAjarExceeded(DOOR_ALERT_TIMEOUT_MS)) {
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
            String msg = "PERIODIC_LOG: Temp=" + String(currentTemp, 2) + "C, Door=" + String(doorStatus ? "OPEN" : "CLOSED");
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
    if (currentMillis - lastSyncCheckTime >= 10000) { // Check every 10 seconds
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
                String syncMsg = "STORED_OFFLINE_LOG: Temp=" + String(packet.temperature, 1) + "C";
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