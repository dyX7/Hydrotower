#include "sensor.hpp"
#include <math.h>

// ===================== GLOBAL DEFINITIONS =====================

float ecTempMeasure = 0;
float phTempMeasure = 0;
float meanTemp = 0;
float ecMeasure = 0;
float phMeasure = 0;
float vEcMeasure = 0;
float vPhMeasure = 0;

MovingMean ecFilter(100);
MovingMean phFilter(100);

MovingMean ecTempFilter(100);
MovingMean phTempFilter(100);

MovingMean vEcFilter(100);
MovingMean vPhFilter(100);

point_t active_calib_point{point_t::COUNT};
bool ph_cal_1_valid{false};
bool ph_cal_2_valid{false};
bool ec_cal_1_valid{false};
bool ec_cal_2_valid{false};
bool active_history{false};

// ===================== HELPERS =====================
float readVoltage(const pin_t& pin) {
    uint32_t millivolts = analogReadMilliVolts(pin.idx);
    return millivolts / 1000.0f;
}

float readTemperature(const pin_t& pin, const temp_cfg_t& cfg)
{
    float v_adc = readVoltage(pin);

    // web_log("temp_v=" + String(v_adc, 3));

    if(v_adc <= 0.0f || v_adc >= VREF)
        return 25.0f;

    // NTC on top, fixed resistor to GND:
    float r_measure =
        cfg.r1_series * (VREF / v_adc - 1.0f);

    if(r_measure <= 0.0f)
        return 25.0f;

    float tempK = 1.0f /
    (
        (1.0f / TEMP0_K) +
        (1.0f / cfg.beta) *
        log(r_measure / cfg.r0_temp)
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

    // 1. RAW voltage (EC channel)
    vEcMeasure = vEcFilter.update(readVoltage(pin));

    web_log("ec_v=" + String(vEcMeasure, 3));

    // 2. Safety: no calibration yet
    if (!ec_cal_1_valid || !ec_cal_2_valid)
        return 0.0f;

    // 3. Convert voltage → EC using calibration
    float ec = linearCalibrate(vEcMeasure, ec_cal_1, ec_cal_2);

    // 4. Temperature compensation (ONLY EC uses this)
    ec = applyEcTemperatureCompensation(ec, ecTempMeasure);

    return ec;
}


// ===================== PH =====================
float readPH(const pin_t& pin) {

    // 1. RAW voltage (PH channel)
    vPhMeasure = vPhFilter.update(readVoltage(pin));

    // 2. Safety: no calibration yet
    if (!ph_cal_1_valid || !ph_cal_2_valid)
        return 0.0f;

    // 3. Convert voltage → pH using calibration
    float ph = linearCalibrate(vPhMeasure, ph_cal_1, ph_cal_2);

    // 4. IMPORTANT:
    // DO NOT apply EC temperature compensation to pH
    // do Nernst Equation for temperature compensation
    // (pH temperature compensation is different chemistry and not linear like EC)

    return ph;
}

// ===================== CALIBRATION SETTERS =====================
void setActiveCalibration(point_t point)
{
    active_calib_point = point;
}

void applyCalibration()
{
    switch(active_calib_point)
    {
        case point_t::PH1:
        {
            ph_cal_1.voltage = vPhFilter.get();
            ph_cal_1.temp = phTempFilter.get();
            ph_cal_1_valid = true;
            savePhCalibration1();
            break;
        }
        case point_t::PH2:
        {
            ph_cal_2.voltage = vPhFilter.get();
            ph_cal_2.temp = phTempFilter.get();
            ph_cal_2_valid = true;
            savePhCalibration2();
            break;
        }
        case point_t::EC1:
        {
            ec_cal_1.voltage = vEcFilter.get();
            ec_cal_1.temp = ecTempFilter.get();
            ec_cal_1_valid = true;
            saveEcCalibration1();
            break;
        }
        case point_t::EC2:
        {
            ec_cal_2.voltage = vEcFilter.get();
            ec_cal_2.temp = ecTempFilter.get();
            ec_cal_2_valid = true;
            saveEcCalibration2();
            break;
        }
    }

    // reset active point
    setActiveCalibration(point_t::COUNT);
}

void tempProcess()
{               
    ecTempMeasure = ecTempFilter.update(readTemperature(adcEcTemp, ec_temp_cfg));
    // phTempMeasure = phTempFilter.update(readTemperature(adcPhTemp, ph_temp_cfg));

    static float tempSimEc = 24.0;
    static float tempSimPh = 24.0;
    tempSimEc += random(-3, 4) / 100.0;
    tempSimPh += random(-3, 4) / 100.0;
    if(tempSimEc < 18.0) tempSimEc = 18.0;
    if(tempSimEc > 30.0) tempSimEc = 30.0;
    if(tempSimPh < 18.0) tempSimPh = 18.0;
    if(tempSimPh > 30.0) tempSimPh = 30.0;
    
    // ecTempMeasure = tempSimEc;
    // phTempMeasure = tempSimPh;

    meanTemp = ecTempMeasure; // (ecTempMeasure + phTempMeasure) / 2.0f;
}

// ===================== SENSOR PROCESS =====================
enum phase_t { OFF, CONTROL, WAIT, READ };

void sensorProcess() {

    static bool _read_sensors = false;
    static bool polarity = false;
    static phase_t phase = CONTROL;

    switch (phase) {

        case OFF:
        {
            disableMeasurement();
            phase = CONTROL;
            break;
        }
        case CONTROL:
        {
            polarity = !polarity;
            gpioEcMeasure1.set(!polarity);
            gpioEcMeasure2.set(polarity);
            polarity ? _read_sensors = true : _read_sensors = false;
            phase = WAIT;
            break;
        }
        case WAIT:
        {
            phase = READ;
            break;
        }
        case READ:
        {
            if (_read_sensors) {

                led1.set(true);
                _read_sensors = false;

                // process temp before sensors
                tempProcess();

                ecMeasure = ecFilter.update(readEC(adcEc));
                phMeasure = phFilter.update(readPH(adcPh));

                // in case of calibration
                if(active_calib_point != point_t::COUNT)
                {
                    switch(active_calib_point)
                    {
                        case point_t::PH1:
                        case point_t::PH2:
                        {
                            vPhFilter.update(vPhMeasure);
                            phTempFilter.update(phTempMeasure);
                            break;
                        }
                        case point_t::EC1:
                        case point_t::EC2:
                        {
                            vEcFilter.update(vEcMeasure);
                            ecTempFilter.update(ecTempMeasure);
                            break;
                        }
                    }
                }
                else if(active_history)
                {
                    web_add_data_hist(ecMeasure, phMeasure, meanTemp);
                }

                led1.set(false);
            }
            phase = OFF;
            break;
        }
    }
}

 void disableMeasurement()
 {
    gpioEcMeasure1.set(false);
    gpioEcMeasure2.set(false);
 }