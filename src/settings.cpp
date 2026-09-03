#include "settings.hpp"

// ------------ Status ----------------
const pin_t waterLow {22, "WATER_LOW"};
const pin_t deviceRelais {23, "EN_DEVICES"};

// ------------ EC / pH Control ----------------
const pin_t gpioEc1 {32, "EC1"};
const pin_t gpioEc2 {33, "EC2"};

// ------------ Main Pump ----------------
const pin_t gpioMot1 {21, "MOT_MAIN"};

// ------------ pH+ Pump ----------------
const pin_t pinPwmMot2Dose {25, "MOT_PH+_Dose"};
const pin_t pinPwmMot2Rev  {26, "MOT_PH+_Rev"};

// ------------ pH- Pump ----------------
const pin_t pinPwmMot3Dose {27, "MOT_PH-_Dose"};
const pin_t pinPwmMot3Rev  {14, "MOT_PH-_Rev"}; 

// ------------ Fertilizer A ----------------
const pin_t pinPwmMot4Dose {19, "MOT_FERT_A_Dose"};
const pin_t pinPwmMot4Rev  {18, "MOT_FERT_A_Rev"};

// ------------ Fertilizer B ----------------
const pin_t pinPwmMot5Dose {5, "MOT_FERT_B_Dose"};
const pin_t pinPwmMot5Rev  {17, "MOT_FERT_B_Rev"};

// ------------ Analog Sensors ----------------
const pin_t adcPh     {34, "PH"};        // GPIO35 | ADC1_CH7 | INPUT ONLY
const pin_t adcEc     {35, "EC"};        // GPIO39 | ADC1_CH3 | INPUT ONLY
const pin_t adcEcTemp {39, "EC_T"};      // GPIO34 | ADC1_CH6 | INPUT ONLY

// ------------ UNUSED / BOOT STRAP PINS ------------
const pin_t gpio0  {0,  "GPIO0"};    // BOOT MODE | STRAP PIN | AVOID
const pin_t gpio2  {2,  "GPIO2"};    // BOOT STRAP | ADC2_CH2 | AVOID
const pin_t gpio4  {4,  "GPIO4"};    // BOOT STRAP | ADC2_CH0 | TOUCH0 | AVOID
const pin_t gpio12 {12, "GPIO12"};   // BOOT STRAP | ADC2_CH5 | TOUCH5 | AVOID
const pin_t gpio15 {15, "GPIO15"};   // BOOT STRAP | ADC2_CH3 | AVOID      
const pin_t gpio14 {13, ""};         // GPIO14 | ADC2_CH6 | PWM | SAFE
const pin_t gpio5  {16,  ""};


// ---------------- GPIO ----------------
gpio_in_t waterLevelLow{waterLow};
gpio_out_t gpioEnDevices{deviceRelais};

gpio_out_t gpioEcMeasure1{gpioEc1};
gpio_out_t gpioEcMeasure2{gpioEc2};

gpio_out_t gpioMainPump{gpioMot1};

pwm_out_t pwmPHplusDose(pinPwmMot2Dose, 1,  MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmPHplusRev(pinPwmMot2Rev, 2,    MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmPHminusDose(pinPwmMot3Dose, 3, MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmPHminusRev(pinPwmMot3Rev, 4,   MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmFertilizerADose(pinPwmMot4Dose, 5, MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmFertilizerARev(pinPwmMot4Rev, 6,   MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmFertilizerBDose(pinPwmMot5Dose, 7, MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);
pwm_out_t pwmFertilizerBRev(pinPwmMot5Rev, 8,   MOTOR_PWM_FREQ, 8, MOTOR_DUTY_PERCENT, false);


// ---------------- Constants ----------------
const float VREF = 3.3;
const float TEMP0_K = 298.15;
const uint32_t MOTOR_PWM_FREQ = 2000;
const float MOTOR_DUTY_PERCENT = 0.7f;


// ---------------- Calibration Defaults ----------------
cal_point_t ph_cal_1 {7.0, 2.2, 25.0};
cal_point_t ph_cal_2 {4.0, 2.86, 25.0};
cal_point_t ec_cal_1 {1.41, 2.50, 25.0};
cal_point_t ec_cal_2 {12.88, 3.00, 25.0};

const temp_cfg_t ec_temp_cfg {10000.0, 3950.0, 9860.0};
// const temp_cfg_t ph_temp_cfg {10000.0, 3950.0, 10000.0};