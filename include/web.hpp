#pragma once
#include <Arduino.h>

void web_init();
void web_loop();

// data logging
void web_add_data(float ec, float ph, float temp);
void web_log(const String &msg);

// shared command from web → main
enum system_cmd_t {
  CMD_NONE,

  CMD_IDLE,
  CMD_WATER,
  CMD_MEASURE,
  CMD_REGULATE,
  CMD_FLUSH,
  CMD_CALIBRATE,
};

extern volatile system_cmd_t system_cmd;

extern uint8_t img_test[];
extern const char img_idle[];
extern const char img_water[];
extern const char img_measure[];
extern const char img_regulate[];
extern const char img_flush[];
extern const char img_calibrate[];