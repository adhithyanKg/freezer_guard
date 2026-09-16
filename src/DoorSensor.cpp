#include "DoorSensor.h"

DoorSensor::DoorSensor(uint8_t pin) : pin(pin), doorOpenStartTime(0), alertTriggered(false) {}

void DoorSensor::begin() {
    pinMode(pin, INPUT_PULLUP);
}

bool DoorSensor::isOpen() {
    // For MC-38 (NC): HIGH = Door Open (magnet away, contacts closed to GND)
    return (digitalRead(pin) == HIGH);
}

bool DoorSensor::isAjarExceeded(unsigned long timeoutMs) {
    if (isOpen()) {
        if (doorOpenStartTime == 0) {
            doorOpenStartTime = millis();
        }
        if ((millis() - doorOpenStartTime >= timeoutMs) && !alertTriggered) {
            return true;
        }
    } else {
        doorOpenStartTime = 0;
        alertTriggered = false;
    }
    return false;
}

void DoorSensor::resetAlertFlag() {
    alertTriggered = false;
}

void DoorSensor::markAlertTriggered() {
    alertTriggered = true;
}