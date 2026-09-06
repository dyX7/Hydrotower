#pragma once
#include "pin.hpp"

enum class pumps_t : uint8_t
{
  MAIN_PUMP,
  PH_PLUS,
  PH_MINUS,
  FERTILIZER_A,
  FERTILIZER_B
};

enum class pump_dir : uint8_t
{
  STOP,
  PUMP,
  REVERSE
};

// ------------ GPIO IN ----------------
extern const pin_t waterLow;

// ------------ GPIO OUT ----------------
extern const pin_t deviceRelais;
extern const pin_t gpioEc1;
extern const pin_t gpioEc2;
extern const pin_t pinPwmMot1;
extern const pin_t pinPwmMot2Dose;
extern const pin_t pinPwmMot2Rev;
extern const pin_t pinPwmMot3Dose;
extern const pin_t pinPwmMot3Rev;
extern const pin_t pinPwmMot4Dose;
extern const pin_t pinPwmMot4Rev;
extern const pin_t pinPwmMot5Dose;
extern const pin_t pinPwmMot5Rev;

// ----------- Analog Pins ----------------
extern const pin_t adcEc;
extern const pin_t adcPh;
extern const pin_t adcEcTemp;

// ---------------- GPIO ----------------

extern gpio_out_t gpioEcMeasure1;
extern gpio_out_t gpioEcMeasure2;
extern gpio_out_t gpioMainPump;
extern gpio_out_t gpioEnPumps;

extern gpio_in_t gpioTankStatus;

extern pwm_out_t pwmPHplusDose;
extern pwm_out_t pwmPHplusRev;
extern pwm_out_t pwmPHminusDose;
extern pwm_out_t pwmPHminusRev;
extern pwm_out_t pwmFertilizerADose;
extern pwm_out_t pwmFertilizerARev;
extern pwm_out_t pwmFertilizerBDose;
extern pwm_out_t pwmFertilizerBRev;



// ---------------- Config ----------------
struct temp_cfg_t {
  const float r0_temp;
  const float beta;
  const float r1_series;
};

struct cal_point_t {
  bool valid;
  float value;
  float voltage;
  float temp;
};

// ---------------- Constants ----------------
extern const float VREF;
extern const float TEMP0_K;
extern const uint32_t MOTOR_PWM_FREQ;
extern const float MOTOR_DUTY_PERCENT;

// ---------------- Calibration Defaults ----------------
extern cal_point_t ph_cal_1;
extern cal_point_t ph_cal_2;
extern cal_point_t ec_cal_1;
extern cal_point_t ec_cal_2;

extern const temp_cfg_t ec_temp_cfg;
// extern const temp_cfg_t ph_temp_cfg;