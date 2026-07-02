#pragma once
#include <Arduino.h>
#include <settings.hpp>

void web_init();
void web_loop();

// data logging
void web_add_data_hist(float ec, float ph, float temp);
extern void clearEcHistory();
extern void clearPhHistory();
void web_log(const String &msg);
void web_pumps(pumps_t pump, pump_dir dir);

extern void savePhCalibration1();
extern void savePhCalibration2();
extern void saveEcCalibration1();
extern void saveEcCalibration2();

// shared command from web → main
enum system_cmd_t {
  CMD_NONE,

  CMD_IDLE,
  CMD_STOP,

  CMD_WATER,
  CMD_MEASURE,
  CMD_REGULATE,
  CMD_FLUSH,
  CMD_CALIBRATE_EC1,
  CMD_CALIBRATE_EC2,
  CMD_CALIBRATE_PH1,
  CMD_CALIBRATE_PH2
};

extern system_cmd_t system_cmd;