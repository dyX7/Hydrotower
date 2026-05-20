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
    case CMD_STOP:
    {
      web_log("Manual STOP");
      fsm.transitionTo(&STATE_STOP);
      break;
    }
    case CMD_WATER:
    {
      web_log("Manual WATERING");
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
    case CMD_CALIBRATE_EC:
    {
      web_log("Manual CALIBRATE EC");
      fsm.transitionTo(&STATE_CALIBRATE_EC);
      break;
    }
        case CMD_CALIBRATE_PH:
    {
      web_log("Manual CALIBRATE PH");
      fsm.transitionTo(&STATE_CALIBRATE_PH);
      break;
    }
}
system_cmd = CMD_NONE;

}