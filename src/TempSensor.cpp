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
        return INVALID_TEMPERATURE_C; // Error code
    }
    return temp;
}