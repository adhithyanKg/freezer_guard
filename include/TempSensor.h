#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

#define INVALID_TEMPERATURE_C -999.0f
#define TEMPERATURE_DECIMAL_PLACES 2

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