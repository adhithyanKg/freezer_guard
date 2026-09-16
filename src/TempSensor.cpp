#include "TempSensor.h"

TempSensor::TempSensor(uint8_t pin) : oneWire(pin), sensors(&oneWire) {}

void TempSensor::begin() {
    sensors.begin();
}

float TempSensor::getTemperatureC() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    // Ignore invalid sensor reads
    if (temp == -127.0f || temp == 85.0f) {
        return -999.0f; // Error code
    }
    return temp;
}