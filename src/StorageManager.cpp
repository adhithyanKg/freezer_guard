#include "StorageManager.h"

static size_t readOffset = 0;

static bool bufferFileExists(const char* path) {
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        return false;
    }

    File entry = root.openNextFile();
    while (entry) {
        const char* entryName = entry.name();
        const char* pathName = path[0] == '/' ? path + 1 : path;
        bool matches = strcmp(entryName, path) == 0 || strcmp(entryName, pathName) == 0;
        entry.close();
        if (matches) {
            root.close();
            return true;
        }
        entry = root.openNextFile();
    }

    root.close();
    return false;
}

bool StorageManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[Storage] LittleFS Mount Failed!");
        return false;
    }
    Serial.println("[Storage] LittleFS Mounted Successfully.");
    return true;
}

bool StorageManager::saveReading(float temp, bool doorOpen) {
    File file = LittleFS.open(BUFFER_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("[Storage] Failed to open buffer file for appending");
        return false;
    }

    DataPacket packet;
    packet.temperature = temp;
    packet.doorOpen = doorOpen;
    packet.timestamp = millis();

    file.write((uint8_t*)&packet, sizeof(DataPacket));
    file.close();

    Serial.printf("[Storage] Buffered offline reading to flash (Temp: %.2f C)\n", temp);
    return true;
}

bool StorageManager::hasBufferedData() {
    // 1. Guard check: return false immediately without trying to open file
    if (!bufferFileExists(BUFFER_FILE)) {
        readOffset = 0;
        return false;
    }
    return getBufferedCount() > 0;
}

size_t StorageManager::getBufferedCount() {
    // 2. Guard check: do not open if file doesn't exist
    if (!bufferFileExists(BUFFER_FILE)) {
        readOffset = 0;
        return 0;
    }

    File file = LittleFS.open(BUFFER_FILE, FILE_READ);
    if (!file) {
        readOffset = 0;
        return 0;
    }

    size_t totalSize = file.size();
    file.close();

    if (readOffset >= totalSize) {
        LittleFS.remove(BUFFER_FILE);
        readOffset = 0;
        return 0;
    }

    return (totalSize - readOffset) / sizeof(DataPacket);
}

bool StorageManager::getOldestReading(DataPacket &packet) {
    if (!bufferFileExists(BUFFER_FILE)) {
        readOffset = 0;
        return false;
    }

    File file = LittleFS.open(BUFFER_FILE, FILE_READ);
    if (!file) return false;

    size_t totalSize = file.size();
    if (readOffset + sizeof(DataPacket) > totalSize) {
        file.close();
        return false;
    }

    file.seek(readOffset);
    size_t bytesRead = file.read((uint8_t*)&packet, sizeof(DataPacket));
    file.close();

    return (bytesRead == sizeof(DataPacket));
}

void StorageManager::popOldestReading() {
    if (!bufferFileExists(BUFFER_FILE)) {
        readOffset = 0;
        return;
    }

    readOffset += sizeof(DataPacket);

    File file = LittleFS.open(BUFFER_FILE, FILE_READ);
    if (file) {
        size_t totalSize = file.size();
        file.close();

        if (readOffset >= totalSize) {
            LittleFS.remove(BUFFER_FILE);
            readOffset = 0;
            Serial.println("[Storage] Buffer completely flushed. File removed.");
        }
    } else {
        readOffset = 0;
    }
}

void StorageManager::clearBuffer() {
    if (LittleFS.exists(BUFFER_FILE)) {
        LittleFS.remove(BUFFER_FILE);
        Serial.println("[Storage] Buffer file deleted from flash.");
    }
    readOffset = 0;
}