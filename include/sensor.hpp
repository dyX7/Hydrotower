#pragma once
#include <Arduino.h>
#include "settings.hpp"
#include "algorithm.hpp"
#include <web.hpp>

// ===================== GLOBAL STATE =====================
extern bool read_sensors;

extern float tempMeasure;

extern float ecMeasure;
extern float phMeasure;

extern float vMilliEcInterrupt;
extern float vEcMeasure;
extern float vPhMeasure;

extern MovingMean tempFilter;
extern MovingMean vEcFilter;
extern MovingMean vPhFilter;

enum class point_t : uint8_t
{
    PH1,
    PH2,
    EC1,
    EC2,
    COUNT
};

// ===================== SENSOR API =====================
void setupSensors();

float readVoltage(const pin_t& pin);

float readTemperature(const pin_t& pin, const temp_cfg_t& cfg);
float applyEcTemperatureCompensation(float ec, float temp);

float linearCalibrate(
    float voltage,
    const cal_point_t& p1,
    const cal_point_t& p2
);

void logCalPoint(const String& label, const cal_point_t& p);

extern void setActiveCalibration(point_t point);
extern void applyCalibration();

extern void tempProcess();
extern void sensorProcess();
extern void updateSensors();

extern void enableMeasurement();
extern void disableMeasurement();