#pragma once
#include <Arduino.h>
#include "settings.hpp"
#include "algorithm.hpp"
#include <web.hpp>

// ===================== GLOBAL STATE =====================
extern bool _read_sensors;
extern bool active_history;

extern float ecTempMeasure;
extern float phTempMeasure;
extern float ecMeasure;
extern float phMeasure;
extern float vEcMeasure;
extern float vPhMeasure;
extern float meanTemp;

extern MovingMean ecFilter;
extern MovingMean phFilter;
extern MovingMean ecTempFilter;
extern MovingMean phTempFilter;
extern MovingMean ecVoltFilter;
extern MovingMean phVoltFilter;


enum class point_t : uint8_t
{
    PH1,
    PH2,
    EC1,
    EC2,
    COUNT
};

// ===================== SENSOR API =====================
float readVoltage(const pin_t& pin);

float readTemperature(const pin_t& pin, const temp_cfg_t& cfg);
float applyEcTemperatureCompensation(float ec, float temp);

float linearCalibrate(
    float voltage,
    const cal_point_t& p1,
    const cal_point_t& p2
);

float readEC(const pin_t& pin);
float readPH(const pin_t& pin);

void logCalPoint(const String& label, const cal_point_t& p);

extern void setActiveCalibration(point_t point);
extern void applyCalibration();

extern void tempProcess();
extern void sensorProcess();
extern void sensorRead();