#pragma once
#include <Arduino.h>
#include "settings.hpp"
#include "algorithm.hpp"
#include <web.hpp>

// ===================== GLOBAL STATE =====================
extern bool _read_sensors;

extern float ecTempMeasure;
extern float phTempMeasure;
extern float ecMeasure;
extern float phMeasure;

extern MovingMean ecFilter;
extern MovingMean phFilter;


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

void setPhCalibration(int index, float ph_value);
void setEcCalibration(int index, float ec_value);

extern void sensorProcess();
extern void sensorRead();