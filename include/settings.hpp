#pragma once
#include "pin.hpp"

// ------------ GPIO Pins ----------------
extern const pin_t pinLed2;
extern const pin_t pinLed1;
extern const pin_t gpioEc1;
extern const pin_t gpioEc2;
extern const pin_t gpioPh1;
extern const pin_t gpioPh2;
extern const pin_t pinPwmMot1;
extern const pin_t pinPwmMot2;
extern const pin_t pinPwmMot3;
extern const pin_t pinPwmMot4;
extern const pin_t pinPwmMot5;

extern uint8_t img_test[];
extern const char img_idle[];
extern const char img_water[];
extern const char img_measure[];
extern const char img_regulate[];
extern const char img_flush[];
extern const char img_calibrate[];

// ----------- Analog Pins ----------------
extern const pin_t adcEc;
extern const pin_t adcPh;
extern const pin_t adcEcTemp;
extern const pin_t adcPhTemp;

// ---------------- GPIO ----------------
extern gpio_out_t led1;
extern gpio_out_t led2;

extern gpio_out_t gpioEcMeasure1;
extern gpio_out_t gpioEcMeasure2;
extern gpio_out_t gpioPhMeasure1;
extern gpio_out_t gpioPhMeasure2;
extern gpio_out_t pwmMotor1;
extern gpio_out_t pwmMotor2;
extern gpio_out_t pwmMotor3;
extern gpio_out_t pwmMotor4;
extern gpio_out_t pwmMotor5;

// ---------------- Config ----------------
struct temp_cfg_t {
  const float r0_temp;
  const float beta;
  const float r1_series;
};

struct cal_point_t {
  float temp;
  float v_p1;
  float v_p2;
};

// ---------------- Constants ----------------
extern const float VREF;
extern const int ADC_RES;
extern const float TEMP0_K;

extern const float R_SERIES_EC;
extern const float K_CELL;
extern const float ALPHA;

extern const float ph_p1;
extern const float ph_p2;

extern const float EC_A;
extern const float EC_B;

// ---------------- Calibration ----------------
extern const cal_point_t ph_cal_1;
extern const cal_point_t ph_cal_2;

extern const float slopePh1;
extern const float slopePh2;
extern const float offsetPh1;
extern const float offsetPh2;

extern const cal_point_t ec_cal_1;
extern const cal_point_t ec_cal_2;

extern const float slopeEc1;
extern const float slopeEc2;
extern const float offsetEc1;
extern const float offsetEc2;

extern const temp_cfg_t ec_temp_cfg;
extern const temp_cfg_t ph_temp_cfg;

extern const int CTRL_FREQ;
extern const float CTRL_TIME;
extern const float MEASURE_TIME;