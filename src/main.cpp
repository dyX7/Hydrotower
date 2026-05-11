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

void disableLed1() 
{
  led1.set(false);
}

void toggleLed2() 
{
  static bool state = false;
  led2.set(state);
  state = !state;
}

void disableLed2() 
{
  led2.set(false);
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
    
      // Ec control
      gpioEcMeasure1.set(!polarity);
      gpioEcMeasure2.set(polarity);

      // Ph control
      gpioPhMeasure1.set(!polarity);
      gpioPhMeasure2.set(polarity);
      
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

        // remove later
        static float ecSim = 1.8;
        static float phSim = 6.0;
        ecSim += random(-5, 6) / 100.0;
        phSim += random(-3, 4) / 100.0;
        // clamp values
        if(ecSim < 0.8) ecSim = 0.8;
        if(ecSim > 2.4) ecSim = 2.4;
        if(phSim < 5.5) phSim = 5.5;
        if(phSim > 7.0) phSim = 7.0;

        ec = ecSim;
        ph = phSim;

        web_add_data(ec, ph);

        // Serial.print("EC: "); Serial.print(ec);
        // Serial.print(" | PH: "); Serial.println(ph);
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
  web_loop();

  // handle web commands
  switch(system_cmd)
  {
    case CMD_IDLE:
    {
      web_log("Manual IDLE");
      fsm.transitionTo(&STATE_IDLE);
      break;
    }
    case CMD_WATER:
    {
      web_log("Watering ENABLED");
      fsm.transitionTo(&STATE_WATERING);
      break;
    }
    case CMD_MEASURE:
    {
      web_log("Manual MEASURE");
      fsm.transitionTo(&STATE_MEASURE);
      break;
    }
    case CMD_REGULATE:
    {
      web_log("Manual REGULATE");
      fsm.transitionTo(&STATE_REGULATE);
      break;
    }
    case CMD_FLUSH:
    {
      web_log("Manual FLUSH");
      fsm.transitionTo(&STATE_FLUSH);
      break;
    }
    case CMD_CALIBRATE:
    {
      web_log("Manual CALIBRATE");
      fsm.transitionTo(&STATE_CALIBRATE);
      break;
    }
}
system_cmd = CMD_NONE;

}