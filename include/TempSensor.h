#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TempSensor {
private:
    OneWire oneWire;
    DallasTemperature sensors;

public:
    TempSensor(uint8_t pin);
    void begin();
    float getTemperatureC();
};

#endif