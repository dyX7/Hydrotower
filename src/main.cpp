#include <Arduino.h>
#include "fsm.hpp"
#include "scheduler.hpp"
#include "pin.hpp"
#include "trace.hpp"
#include "web.hpp"

// ---------------- Base Instances ----------------
scheduler_t scheduler;
fsm_t fsm{scheduler};

// ---------------- Variables ----------------
float ecTemp = 0;
float ec = 0;
float phTemp = 0;
float ph = 0;
bool _read_sensors{false};

// ---------------- Helper ----------------
float readVoltage(const pin_t& pin)
{
  auto value = analogRead(pin.idx);
  return (value * VREF) / ADC_RES;
}

// ---------------- Temperature ----------------
float readTemperature(const pin_t& pin, const temp_cfg_t &cfg)
{
  auto value = analogRead(pin.idx);
  float r_measure = cfg.r1_series * (VREF / value - 1.0);
  float tempK = 1.0 / ((1.0 / TEMP0_K) + (1.0 / cfg.beta) * log(r_measure / cfg.r0_temp));
  return tempK - 273.15;
}

// ---------------- EC ----------------
float readEC(const pin_t& pin, float temperature)
{
  float voltage = readVoltage(pin);
  float R = R_SERIES_EC * (VREF / voltage - 1.0);
  float G = 1.0 / R;
  float kappa = K_CELL * G;
  float kappa_uS = kappa * 1e6;
  float kappa25 = kappa_uS / (1.0 + ALPHA * (temperature - 25.0));
  return kappa25;
}

// ---------------- PH ----------------
float readPH(const pin_t& pin, float temperature)
{
  float voltage = readVoltage(pin);
  float t_ratio = (temperature - ph_cal_1.temp) / (ph_cal_2.temp - ph_cal_1.temp);
  float slope  = slopePh1  + (slopePh2  - slopePh1)  * t_ratio;
  float offset = offsetPh1 + (offsetPh2 - offsetPh1) * t_ratio;

  return slope * voltage + offset;
}

// ---------------- LED ----------------
void toggleLed1() 
{
  static bool state = false;
  led1.set(state);
  state = !state;
}

void toggleLed2() 
{
  static bool state = false;
  led2.set(state);
  state = !state;
}

// ---------------- Sensor Process ----------------
enum phase_t { CONTROL, WAIT, READ };

void sensorProcess()
{
  static bool polarity = false;
  static phase_t phase{CONTROL};

  switch(phase)
  {
    case CONTROL:
    {
      polarity = !polarity;
      pwmEcMeasure.set(!polarity);
      pwmPhMeasure.set(!polarity);
      pwmEcLow.set(polarity);
      pwmPhLow.set(polarity);
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
      if(_read_sensors)
      {
        ecTemp = readTemperature(adcEcTemp, ec_temp_cfg);
        ec     = readEC(adcEc, ecTemp);
        phTemp = readTemperature(adcPhTemp, ph_temp_cfg);
        ph     = readPH(adcPh, phTemp);

        _read_sensors = false;

        web_add_data(ec, ph);

        Serial.print("EC: "); Serial.print(ec);
        Serial.print(" | PH: "); Serial.println(ph);
      }

      phase = CONTROL;
      break;
    }
  }
}

// ---------------- Trigger ----------------
void sensorRead()
{
  _read_sensors = true;
}

// ---------------- Setup ----------------
void setup() 
{
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  web_init();
  fsm.begin(&STATE_INIT);

  Serial.println("setup end!");
}

// ---------------- Loop ----------------
void loop() 
{
  fsm.poll();
  scheduler.poll();
  sensorProcess();

if(system_cmd == CMD_WATER_ON)
{
  system_cmd = CMD_NONE;
  Serial.println("FSM -> WATER ON");
  fsm.force_watering = true;
  fsm.transitionTo(&STATE_WATERING);
}

if(system_cmd == CMD_WATER_OFF)
{
  system_cmd = CMD_NONE;
  Serial.println("FSM -> WATER OFF");
  fsm.watering_done = true;
  fsm.transitionTo(&STATE_IDLE);
}
}