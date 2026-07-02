#include <Arduino.h>
#include "fsm.hpp"
#include "scheduler.hpp"
#include "pin.hpp"
#include "trace.hpp"
#include "web.hpp"
#include "sensor.hpp"

// ---------------- Base Instances ----------------
scheduler_t scheduler;
fsm_t fsm{scheduler};

// ---------------- Setup ----------------
void setup() 
{
  Serial.begin(115200);

  setupSensors();

  web_init();
  fsm.begin(&STATE_INIT);

  Serial.println("setup end!");
}

// ---------------- Loop ----------------
void loop() 
{
  updateSensors();
  fsm.poll();
  scheduler.poll();
  web_loop();

  // handle web commands
  switch(system_cmd)
  {
    case CMD_IDLE:
    {
      web_log("Manual IDLE");
      setStopLock(false);
      fsm.transitionTo(&STATE_IDLE);
      break;
    }
    case CMD_STOP:
    {
      web_log("Manual STOP");
      setStopLock(true);
      fsm.transitionTo(&STATE_STOP);
      break;
    }
    case CMD_WATER:
    {
      web_log("Manual WATERING");
      setStopLock(false);
      fsm.transitionTo(&STATE_WATERING);
      break;
    }
    case CMD_MEASURE:
    {
      web_log("Manual MEASURE");
      setStopLock(false);
      fsm.transitionTo(&STATE_MEASURE);
      break;
    }
    case CMD_REGULATE:
    {
      web_log("Manual REGULATE");
      setStopLock(false);
      fsm.transitionTo(&STATE_REGULATE);
      break;
    }
    case CMD_FLUSH:
    {
      web_log("Manual FLUSH");
      setStopLock(false);
      fsm.transitionTo(&STATE_FLUSH);
      break;
    }
    case CMD_CALIBRATE_EC1:
    {
      setActiveCalibration(point_t::EC1);
      fsm.transitionTo(&STATE_CALIBRATE);
      break;
    }
    case CMD_CALIBRATE_EC2:
    {
      setActiveCalibration(point_t::EC2);
      fsm.transitionTo(&STATE_CALIBRATE);
      break;
    }
    case CMD_CALIBRATE_PH1:
    {
      setActiveCalibration(point_t::PH1);
      fsm.transitionTo(&STATE_CALIBRATE);
      break;
    }
    case CMD_CALIBRATE_PH2:
    {
      setActiveCalibration(point_t::PH2);
      fsm.transitionTo(&STATE_CALIBRATE);
      break;
    }
  }
  system_cmd = CMD_NONE;

}