#include "sensor.hpp"
#include <math.h>

// ===================== GLOBAL DEFINITIONS =====================
bool _read_sensors = false;

float ecTempMeasure = 0;
float phTempMeasure = 0;
float ecMeasure = 0;
float phMeasure = 0;

MovingMean ecFilter(10);
MovingMean phFilter(10);

// external calibration/state (must exist in another .cpp or settings.cpp)
bool ph_cal_valid_1{false};
bool ph_cal_valid_2{false};
bool ec_cal_valid_1{false};
bool ec_cal_valid_2{false};

// ===================== HELPERS =====================
float readVoltage(const pin_t& pin) {
    auto value = analogRead(pin.idx);
    return (value * VREF) / ADC_RES;
}

// ===================== TEMPERATURE =====================
float readTemperature(const pin_t& pin, const temp_cfg_t &cfg) {
    auto value = analogRead(pin.idx);

    float r_measure = cfg.r1_series * (VREF / value - 1.0f);
    float tempK = 1.0f / (
        (1.0f / TEMP0_K) +
        (1.0f / cfg.beta) * log(r_measure / cfg.r0_temp)
    );

    return tempK - 273.15f;
}

float applyEcTemperatureCompensation(float ec, float temp)
{
    const float alpha = 0.02f; // typical EC temp coefficient (2% per °C)

    return ec / (1.0f + alpha * (temp - 25.0f));
}


// ===================== CALIBRATION =====================
float linearCalibrate(float voltage, const cal_point_t& p1, const cal_point_t& p2) {
    float dv = p2.voltage - p1.voltage;
    if (fabs(dv) < 0.0001f) return p1.value;

    float slope = (p2.value - p1.value) / dv;
    float offset = p1.value - slope * p1.voltage;

    return slope * voltage + offset;
}

// ===================== EC =====================
float readEC(const pin_t& pin) {
    if (!ec_cal_valid_1 || !ec_cal_valid_2) return 0.0f;

    float voltage = readVoltage(pin);
    float ec = linearCalibrate(voltage, ec_cal_1, ec_cal_2);

    return applyEcTemperatureCompensation(ec, ecTempMeasure);
}


// ===================== PH =====================
float readPH(const pin_t& pin) {
    if (!ph_cal_valid_1 || !ph_cal_valid_2) return 0.0f;

    float voltage = readVoltage(pin);
    float ph = linearCalibrate(voltage, ph_cal_1, ph_cal_2);

    return applyEcTemperatureCompensation(ph, phTempMeasure);
}

// ===================== CALIBRATION SETTERS =====================
void setPhCalibration(int index, float ph_value) {
    cal_point_t point;
    point.value = ph_value;
    point.voltage = readVoltage(adcPh);
    point.temp = phTempMeasure;

    if (index == 1) {
        ph_cal_1 = point;
        ph_cal_valid_1 = true;
    } else {
        ph_cal_2 = point;
        ph_cal_valid_2 = true;
    }
}

void setEcCalibration(int index, float ec_value) {
    cal_point_t point;
    point.value = ec_value;
    point.voltage = readVoltage(adcEc);
    point.temp = ecTempMeasure;

    if (index == 1) {
        ec_cal_1 = point;
        ec_cal_valid_1 = true;
    } else {
        ec_cal_2 = point;
        ec_cal_valid_2 = true;
    }
}


// ===================== SENSOR PROCESS =====================
enum phase_t { CONTROL, WAIT, READ };

void sensorProcess() {
    static bool polarity = false;
    static phase_t phase = CONTROL;

    switch (phase) {

        case CONTROL:
            polarity = !polarity;

            gpioEcMeasure1.set(!polarity);
            gpioEcMeasure2.set(polarity);

            gpioPhMeasure1.set(!polarity);
            gpioPhMeasure2.set(polarity);

            phase = WAIT;
            break;

        case WAIT:
            phase = READ;
            break;

        case READ:
            if (_read_sensors) {

                ecTempMeasure = readTemperature(adcEcTemp, ec_temp_cfg);
                phTempMeasure = readTemperature(adcPhTemp, ph_temp_cfg);

                ecMeasure = ecFilter.update(readEC(adcEc));
                phMeasure = phFilter.update(readPH(adcPh));

                _read_sensors = false;

                // remove later
                static float ecSim = 1.8;
                static float phSim = 6.0;
                static float tempSimEc = 24.0;
                static float tempSimPh = 24.0;

                ecSim += random(-5, 6) / 100.0;
                phSim += random(-3, 4) / 100.0;
                tempSimEc += random(-3, 4) / 100.0;
                tempSimPh += random(-3, 4) / 100.0;

                // clamp values
                if(ecSim < 0.8) ecSim = 0.8;
                if(ecSim > 2.4) ecSim = 2.4;
                if(phSim < 5.5) phSim = 5.5;
                if(phSim > 7.0) phSim = 7.0;
                if(tempSimEc < 18.0) tempSimEc = 18.0;
                if(tempSimEc > 30.0) tempSimEc = 30.0;
                if(tempSimPh < 18.0) tempSimPh = 18.0;
                if(tempSimPh > 30.0) tempSimPh = 30.0;

                ecMeasure = ecSim;
                phMeasure = phSim;
                ecTempMeasure = tempSimEc;
                phTempMeasure = tempSimPh;

                auto meanTemp = (ecTempMeasure + phTempMeasure) / 2.0f;

                web_add_data(ecMeasure, phMeasure, meanTemp);
            }
            phase = CONTROL;
            break;
    }
}

void sensorRead() {
    _read_sensors = true;
}