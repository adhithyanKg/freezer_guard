#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

struct DataPacket {
    float temperature;
    bool doorOpen;
    unsigned long timestamp;
};

class StorageManager {
public:
    bool begin();
    bool saveReading(float temp, bool doorOpen);
    bool hasBufferedData();
    bool getOldestReading(DataPacket &packet);
    void popOldestReading();
    size_t getBufferedCount();
    void clearBuffer();

private:
    const char* BUFFER_FILE = "/buffer.dat";
};

#endif