#pragma once
#include <Arduino.h>

void web_init();
void web_loop();

// data logging
void web_add_data(float ec, float ph);
void web_log(const String &msg);

// pump parameters
extern int pump_on_minutes;
extern int pump_cycle_minutes;

// shared command from web → main
enum system_cmd_t {
  CMD_NONE,

  CMD_WATER_ON,
  CMD_WATER_OFF,

  CMD_MEASURE_ON,
  CMD_MEASURE_OFF,

  CMD_REGULATE_ON,
  CMD_REGULATE_OFF,

  CMD_FLUSH_ON,
  CMD_FLUSH_OFF,

  CMD_CALIBRATE_ON,
  CMD_CALIBRATE_OFF
};

extern volatile system_cmd_t system_cmd;