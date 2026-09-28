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
unsigned long last30MinLogTime = 0;
unsigned long lastSyncCheckTime = 0;
unsigned long tempUnsafeStartTime = 0;
unsigned long lastTempAlertAttemptTime = 0;
unsigned long lastTempRecoveryAttemptTime = 0;
bool temperatureIncidentActive = false;
bool temperatureAlertPending = false;
bool temperatureRecoveryPending = false;
float temperatureAlertValue = INVALID_TEMPERATURE_C;
float temperatureRecoveryValue = INVALID_TEMPERATURE_C;
unsigned long lastDoorAlertAttemptTime = 0;
bool doorAlertPending = false;
float doorAlertTemperature = INVALID_TEMPERATURE_C;

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

        // --- RULE 1: Temperature must remain above the limit before alerting ---
        if (currentTemp != INVALID_TEMPERATURE_C && currentTemp > MAX_TEMP_THRESHOLD_C) {
            if (tempUnsafeStartTime == 0) {
                tempUnsafeStartTime = currentMillis;
            }

            if (!temperatureIncidentActive &&
                currentMillis - tempUnsafeStartTime >= TEMP_ALERT_DELAY_MS) {
                temperatureIncidentActive = true;
                temperatureAlertValue = currentTemp;
                temperatureAlertPending = true;
                lastTempAlertAttemptTime = 0;
                Serial.println("[Alert] Temperature incident detected; notification queued.");
            }
        } else if (currentTemp != INVALID_TEMPERATURE_C && currentTemp <= TEMP_RECOVERY_THRESHOLD_C) {
            tempUnsafeStartTime = 0;

            if (temperatureIncidentActive) {
                temperatureIncidentActive = false;
                temperatureRecoveryValue = currentTemp;
                temperatureRecoveryPending = true;
                lastTempRecoveryAttemptTime = 0;
                Serial.println("[Alert] Temperature recovered; recovery notification queued.");
            }
        } else if (currentTemp != INVALID_TEMPERATURE_C) {
            // Temperature left the unsafe range but has not reached the recovery threshold.
            tempUnsafeStartTime = 0;
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (temperatureAlertPending &&
                (lastTempAlertAttemptTime == 0 ||
                 currentMillis - lastTempAlertAttemptTime >= TEMP_ALERT_RETRY_INTERVAL_MS)) {
                lastTempAlertAttemptTime = currentMillis;
                String msg = "ALERT: Temp exceeded threshold! Current: " +
                             String(temperatureAlertValue, TEMPERATURE_DECIMAL_PLACES) + " C";
                Serial.println("[Alert] Sending temperature alert to Webhook...");
                if (alertManager.sendAlert("TEMP_HIGH_ALERT", msg, temperatureAlertValue)) {
                    temperatureAlertPending = false;
                    Serial.println("[Alert] Temperature alert delivered.");
                } else {
                    Serial.println("[Alert] Temperature alert delivery failed; will retry.");
                }
            }

            if (temperatureRecoveryPending &&
                (lastTempRecoveryAttemptTime == 0 ||
                 currentMillis - lastTempRecoveryAttemptTime >= TEMP_ALERT_RETRY_INTERVAL_MS)) {
                lastTempRecoveryAttemptTime = currentMillis;
                String msg = "RECOVERY: Temperature returned to safe range. Current: " +
                             String(temperatureRecoveryValue, TEMPERATURE_DECIMAL_PLACES) + " C";
                Serial.println("[Alert] Sending temperature recovery to Webhook...");
                if (alertManager.sendAlert("TEMP_RECOVERY", msg, temperatureRecoveryValue)) {
                    temperatureRecoveryPending = false;
                    Serial.println("[Alert] Temperature recovery delivered.");
                } else {
                    Serial.println("[Alert] Temperature recovery delivery failed; will retry.");
                }
            }
        }
    }

    // --- RULE 2: Door Alert (Open > 15 Mins) ---
    if (doorSensor.isAjarExceeded(DOOR_OPEN_TIMEOUT_MS)) {
        doorSensor.markAlertTriggered(); // Prevents repeated firing while door stays open
        doorAlertTemperature = tempSensor.getTemperatureC();
        doorAlertPending = true;
        lastDoorAlertAttemptTime = 0;
        Serial.println("[Alert] Door-open incident detected; notification queued.");
    }

    if (WiFi.status() == WL_CONNECTED && doorAlertPending &&
        (lastDoorAlertAttemptTime == 0 ||
         currentMillis - lastDoorAlertAttemptTime >= TEMP_ALERT_RETRY_INTERVAL_MS)) {
        lastDoorAlertAttemptTime = currentMillis;
        String msg = "ALERT: Door has been left open for over 15 minutes! Temp=" +
                     String(doorAlertTemperature, TEMPERATURE_DECIMAL_PLACES) + " C";
        Serial.println("[Alert] Sending door alert to Webhook...");
        if (alertManager.sendAlert("DOOR_OPEN_ALERT", msg, doorAlertTemperature)) {
            doorAlertPending = false;
            Serial.println("[Alert] Door alert delivered.");
        } else {
            Serial.println("[Alert] Door alert delivery failed; will retry.");
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
                String syncMsg = "STORED_OFFLINE_LOG: Temp=" + String(packet.temperature, TEMPERATURE_DECIMAL_PLACES) +
                                 "C, Door=" + String(packet.doorOpen ? "OPEN" : "CLOSED");
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