#ifndef CONFIG_H
#define CONFIG_H

// Network Credentials
#define WIFI_SSID "Pixel 10"
#define WIFI_PASS "Qwerty@123"
// #define WEBHOOK_URL "https://hook.us2.make.com/tos6xbuj8wmxc7e98771fyeo4z74bvb7"
#define WEBHOOK_URL "http://webhook.site/cc5ca017-4c0a-4da9-9561-4af94917f9f3"

#define SERIAL_BAUD_RATE 115200
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_INTERVAL_MS 500
#define HTTP_TIMEOUT_MS 15000

// Hardware Pin Assignments
#define TEMP_SENSOR_PIN 4
#define DOOR_SENSOR_PIN 5
#define SENSOR_CHECK_INTERVAL_MS 2000
#define STORAGE_SYNC_INTERVAL_MS 10000

// Operational Limits
#define MAX_TEMP_THRESHOLD_C -5.0   // Alert if freezer goes above -5°C
#define DOOR_OPEN_TIMEOUT_MS (15UL * 60UL * 1000UL)  // 15 minutes
#define ALERT_COOLDOWN_MS    300000 // 5-minute cooldown between repeated alerts
#define TELEMETRY_INTERVAL_MS (30UL * 60UL * 1000UL)   // 30 Minutes

#endif