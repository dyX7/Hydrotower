#include "sensor.hpp"
#include <math.h>

// ===================== GLOBAL DEFINITIONS =====================

bool ec_read = false;
bool ec_active = false;
bool ph_active = false;

float tempMeasure = 0;
float vTempMeasure = 0;

float ecMeasure = 0;
float phMeasure = 0;

float vEcInterrupt = 0;
float vEcMeasure = 0;
float vPhMeasure = 0;

MovingMean tempFilter(10);
MovingMean vEcFilter(10);
MovingMean vPhFilter(10);

point_t active_calib_point{point_t::DISBALED};

bool calibration_valid()
{
    return ph_cal_1.valid && ph_cal_2.valid && ec_cal_1.valid && ec_cal_2.valid;
}

hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    ecControl();
    portEXIT_CRITICAL_ISR(&timerMux);
}

void ecSetup()
{
    // adc
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // timer interrupt
    timer = timerBegin(0, 80, true); // timer 0, prescaler 80 → 1 µs tick (80MHz / 80 = 1MHz)
    timerAttachInterrupt(timer, &onTimer, true);  // interval in microseconds
    timerAlarmWrite(timer, 250, true); // 250 µs = 4 kHz toggle rate
}

// ===================== HELPERS =====================
float readVoltage(const pin_t& pin) {
    uint32_t millivolts = analogReadMilliVolts(pin.idx);
    return millivolts / 1000.0f;
}

float readTemperature(const pin_t& pin, const temp_cfg_t& cfg)
{
    vTempMeasure = readVoltage(pin);

    // web_log("temp_v=" + String(vTempMeasure, 3));

    if(vTempMeasure <= 0.0f || vTempMeasure >= VREF)
        return 25.0f;

    // NTC on bottom
    float r_measure =
        cfg.r1_series * (vTempMeasure / (VREF - vTempMeasure));

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

// ===================== CALIBRATION SETTERS =====================
void setActiveCalibration(point_t point)
{
    active_calib_point = point;
    web_log("set calibratrion point " + String(static_cast<int>(point)));
}

void applyCalibration()
{
    switch(active_calib_point)
    {
        case point_t::PH1:
        {
            clearPhHistory();
            ph_cal_1.voltage = vPhFilter.get();
            ph_cal_1.temp = tempFilter.get();
            ph_cal_1.valid = true;
            saveCalibrationPh1();
            break;
        }
        case point_t::PH2:
        {
            clearPhHistory();
            ph_cal_2.voltage = vPhFilter.get();
            ph_cal_2.temp = tempFilter.get();
            ph_cal_2.valid = true;
            saveCalibrationPh2();
            break;
        }
        case point_t::EC1:
        {            
            clearEcHistory();
            ec_cal_1.voltage = vEcFilter.get();
            ec_cal_1.temp = tempFilter.get();
            ec_cal_1.valid = true;
            saveCalibrationEc1();
            break;
        }
        case point_t::EC2:
        {
            clearEcHistory();
            ec_cal_2.voltage = vEcFilter.get();
            ec_cal_2.temp = tempFilter.get();
            ec_cal_2.valid = true;
            saveCalibrationEc2();
            break;
        }
    }
}

void tempProcess()
{               
    tempMeasure = tempFilter.update(readTemperature(adcEcTemp, ec_temp_cfg));
}

// ===================== SENSOR PROCESS =====================
enum phase_t { OFF, CONTROL, READ };

void ecControl() {

    static bool polarity = false;
    static phase_t phase = CONTROL;

    switch (phase) {

        case OFF:
        {  
            ec_read = false;
            digitalWrite(gpioEc1.idx, false);
            digitalWrite(gpioEc2.idx, false);
            phase = CONTROL;
            break;
        }
        case CONTROL:
        {
            polarity = !polarity;
            digitalWrite(gpioEc1.idx, !polarity);
            digitalWrite(gpioEc2.idx, polarity);
            polarity ? ec_read = true : ec_read = false;
            phase = READ;
            break;
        }
        case READ:
        {
            phase = OFF;
            break;
        }
    }
}

void ecRead()
{
    if (ec_active && ec_read)
    {
        vEcMeasure = readVoltage(adcEc);
        ec_read = false;
    }
}

void ecProcess()
{
    if (ec_active)
    {
        vEcFilter.update(vEcMeasure);
        
        if (ec_cal_1.valid && ec_cal_2.valid)
        {
            ecMeasure = applyEcTemperatureCompensation(
                    linearCalibrate(
                        vEcFilter.get(), 
                        ec_cal_1,
                        ec_cal_2),
                    tempMeasure);
        }
    }
}

void phReadProcess()
{
    if (ph_active)
    {
        vPhMeasure = readVoltage(adcPh);
        vPhFilter.update(vPhMeasure);

        if (ph_cal_1.valid && ph_cal_2.valid)
        {
            phMeasure = linearCalibrate(
                    vPhFilter.get(), 
                    ph_cal_1,
                    ph_cal_2);
        }
    }
}

void enableEc()
{
    if(!ec_active)
    {
        timerAlarmEnable(timer);
        ec_active = true;
    }
}

void disableEc()
{
    if(ec_active)
    {
        timerAlarmDisable(timer);
        ec_active = false;
    }
    digitalWrite(gpioEc1.idx, false);
    digitalWrite(gpioEc2.idx, false);
}

void enablePh()
{
    ph_active = true;
}

void disablePh()
{
    ph_active = false;
}