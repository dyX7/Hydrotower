#include "sensor.hpp"
#include <math.h>

// ===================== GLOBAL DEFINITIONS =====================

bool read_sensors = false;

float tempMeasure = 0;

float ecMeasure = 0;
float phMeasure = 0;

float vEcInterrupt = 0;
float vEcMeasure = 0;
float vPhMeasure = 0;

MovingMean tempFilter(50);
MovingMean vEcFilter(100);
MovingMean vPhFilter(100);

point_t active_calib_point{point_t::COUNT};
bool ph_cal_1_valid{false};
bool ph_cal_2_valid{false};
bool ec_cal_1_valid{false};
bool ec_cal_2_valid{false};

hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);

    sensorProcess();

    portEXIT_CRITICAL_ISR(&timerMux);
}

void setupSensors()
{
    // adc
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // timer interrupt
    timer = timerBegin(0, 80, true); // timer 0, prescaler 80 → 1 µs tick (80MHz / 80 = 1MHz)
    timerAttachInterrupt(timer, &onTimer, true);  // interval in microseconds
    timerAlarmWrite(timer, 1000, true); // 1000 µs = 1 kHz toggle rate
}

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
            clearPhHistory();
            ph_cal_1.voltage = vPhFilter.get();
            ph_cal_1.temp = tempFilter.get();
            ph_cal_1_valid = true;
            savePhCalibration1();
            break;
        }
        case point_t::PH2:
        {
            clearPhHistory();
            ph_cal_2.voltage = vPhFilter.get();
            ph_cal_2.temp = tempFilter.get();
            ph_cal_2_valid = true;
            savePhCalibration2();
            break;
        }
        case point_t::EC1:
        {            
            clearEcHistory();
            ec_cal_1.voltage = vEcFilter.get();
            ec_cal_1.temp = tempFilter.get();
            ec_cal_1_valid = true;
            saveEcCalibration1();
            break;
        }
        case point_t::EC2:
        {
            clearEcHistory();
            ec_cal_2.voltage = vEcFilter.get();
            ec_cal_2.temp = tempFilter.get();
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
    tempMeasure = tempFilter.update(readTemperature(adcEcTemp, ec_temp_cfg));
}

// ===================== SENSOR PROCESS =====================
enum phase_t { OFF, CONTROL, READ };

void sensorProcess() {

    static bool polarity = false;
    static phase_t phase = CONTROL;

    switch (phase) {

        case OFF:
        {  
            digitalWrite(gpioEc1.idx, false);
            digitalWrite(gpioEc2.idx, false);
            phase = CONTROL;
            break;
        }
        case CONTROL:
        {
            read_sensors = false;
            polarity = !polarity;
            digitalWrite(gpioEc1.idx, !polarity);
            digitalWrite(gpioEc2.idx, polarity);
            phase = READ;
            break;
        }
        case READ:
        {
            polarity ? read_sensors = true : read_sensors = false;
            phase = OFF;
            break;
        }
    }
}

void updateSensors()
{
    if (read_sensors)
    {
        auto ecRaw = readVoltage(adcEc);
        auto phRaw = readVoltage(adcPh);
        digitalWrite(gpioEc1.idx, false);
        digitalWrite(gpioEc2.idx, false);

        tempProcess();
        vEcMeasure = vEcFilter.update(ecRaw);
        vPhMeasure = vPhFilter.update(phRaw);

        if (!ec_cal_1_valid || !ec_cal_2_valid)
        {
            vEcMeasure = 0.0f;
            ecMeasure = 0.0f;
        }
        else
        {
            ecMeasure = linearCalibrate(vEcMeasure, ec_cal_1, ec_cal_2);
            ecMeasure = applyEcTemperatureCompensation(ecMeasure, tempMeasure);
        }

        if (!ph_cal_1_valid || !ph_cal_2_valid)
        {
            vPhMeasure = 0.0f;
            phMeasure = 0.0f;
        }
        else
        {
            phMeasure = linearCalibrate(vPhMeasure, ph_cal_1, ph_cal_2);

            // 4. IMPORTANT:
            // DO NOT apply EC temperature compensation to pH
            // do Nernst Equation for temperature compensation
            // (pH temperature compensation is different chemistry and not linear like EC)
        }


        // in case of calibration
        if(active_calib_point != point_t::COUNT)
        {
            switch(active_calib_point)
            {
                case point_t::PH1:
                case point_t::PH2:
                {
                    vPhFilter.update(vPhMeasure);
                    break;
                }
                case point_t::EC1:
                case point_t::EC2:
                {
                    vEcFilter.update(vEcMeasure);
                    break;
                }
            }
        }

        read_sensors = false;
    }
}

void enableMeasurement()
{
    timerAlarmEnable(timer);
}

void disableMeasurement()
{
    timerAlarmDisable(timer);
}