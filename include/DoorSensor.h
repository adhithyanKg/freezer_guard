#ifndef DOOR_SENSOR_H
#define DOOR_SENSOR_H

#include <Arduino.h>

class DoorSensor {
private:
    uint8_t pin;
    unsigned long doorOpenStartTime;
    bool alertTriggered;

public:
    DoorSensor(uint8_t pin);
    void begin();
    bool isOpen();
    bool isAjarExceeded(unsigned long timeoutMs);
    void resetAlertFlag();
    void markAlertTriggered();
};

#endif