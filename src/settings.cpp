#include "settings.hpp"

// ---------------- Pins ----------------
const pin_t pinLed2         {23,  "LED2"};
const pin_t pinLed1         {22,  "LED1"};

const pin_t pinPwmEcMeasure {0,   "EC1_PWM"};
const pin_t pinPwmEc2       {2,   "EC2_PWM"};
const pin_t pinPwmPhMeasure {0,   "PH1_PWM"};
const pin_t pinPwmPh2       {2,   "PH2_PWM"};

const pin_t pinPwmMot1      {0,   "MOT_MAIN"};
const pin_t pinPwmMot2      {0,   "MOT_PH+"};
const pin_t pinPwmMot3      {0,   "MOT_PH-"};
const pin_t pinPwmMot4      {0,   "MOT_DOSE_A"};
const pin_t pinPwmMot5      {0,   "MOT_DOSE_B"};

const pin_t adcEc           {0,   "EC"};
const pin_t adcPh           {0,   "PH"};
const pin_t adcEcTemp       {0,   "EC_T"};
const pin_t adcPhTemp       {0,   "PH_T"};

// ---------------- GPIO ----------------
gpio_out_t led1{pinLed1, true};
gpio_out_t led2{pinLed2, true};

gpio_out_t pwmEcMeasure{pinPwmEcMeasure};
gpio_out_t pwmEcLow{pinPwmEc2};
gpio_out_t pwmPhMeasure{pinPwmPhMeasure};
gpio_out_t pwmPhLow{pinPwmPh2};

gpio_out_t pwmMotor1{pinPwmMot1};
gpio_out_t pwmMotor2{pinPwmMot2};
gpio_out_t pwmMotor3{pinPwmMot3};
gpio_out_t pwmMotor4{pinPwmMot4};
gpio_out_t pwmMotor5{pinPwmMot5};

// ---------------- Constants ----------------
const float VREF = 3.3;
const int ADC_RES = 4095;
const float TEMP0_K = 298.15;

const float R_SERIES_EC = 10000;
const float K_CELL = 1.0;
const float ALPHA = 0.02;

const float ph_p1 = 7.0;
const float ph_p2 = 4.0;

const float EC_A = 7.0;
const float EC_B = 4.0;

// ---------------- Calibration ----------------
const cal_point_t ph_cal_1 {20.0, 2.50, 3.00};
const cal_point_t ph_cal_2 {30.0, 2.45, 2.95};

const float slopePh1 = (ph_p2 - ph_p1) / (ph_cal_1.v_p2 - ph_cal_1.v_p1);
const float slopePh2 = (ph_p2 - ph_p1) / (ph_cal_2.v_p2 - ph_cal_2.v_p1);
const float offsetPh1 = ph_p1 - slopePh1 * ph_cal_1.v_p1;
const float offsetPh2 = ph_p1 - slopePh2 * ph_cal_2.v_p1;

const cal_point_t ec_cal_1 {20.0, 2.50, 3.00};
const cal_point_t ec_cal_2 {30.0, 2.45, 2.95};

const float slopeEc1 = (EC_B - EC_A) / (ec_cal_1.v_p2 - ec_cal_1.v_p1);
const float slopeEc2 = (EC_B - EC_A) / (ec_cal_2.v_p2 - ec_cal_2.v_p1);
const float offsetEc1 = EC_A - slopeEc1 * ec_cal_1.v_p1;
const float offsetEc2 = EC_A - slopeEc2 * ec_cal_2.v_p1;

const temp_cfg_t ec_temp_cfg {10000.0, 3950.0, 10000.0};
const temp_cfg_t ph_temp_cfg {10000.0, 3950.0, 10000.0};

const int CTRL_FREQ = 5000;
const float CTRL_TIME = 1000000/(CTRL_FREQ*2*3);
const float MEASURE_TIME = 500;