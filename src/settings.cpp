#include "settings.hpp"

// ------------ Status LEDs ----------------
const pin_t pinLed1 {22, "LED1"};        // GPIO22 | I2C SCL | PWM | SAFE
const pin_t pinLed2 {23, "LED2"};        // GPIO23 | SPI MOSI | PWM | SAFE

// ------------ EC / pH Control ----------------
const pin_t gpioEc1 {25, "EC1"};         // GPIO25 | DAC1 | PWM | SAFE
const pin_t gpioEc2 {26, "EC2"};         // GPIO26 | DAC2 | PWM | SAFE

// ------------ Main Pump ----------------
const pin_t gpioMot1 {21, "MOT_MAIN"};  // GPIO21 | I2C SDA | PWM | SAFE

// ------------ pH+ Pump ----------------
const pin_t pinPwmMot2Dose {33, "MOT_PH+_Dose"}; // GPIO33 | ADC1 (RESERVED) | PWM
const pin_t pinPwmMot2Rev  {16, "MOT_PH+_Rev"};  // GPIO16 | UART2 RX | PWM

// ------------ pH- Pump ----------------
const pin_t pinPwmMot3Dose {17, "MOT_PH-_Dose"}; // GPIO17 | UART2 TX | PWM
const pin_t pinPwmMot3Rev  {18, "MOT_PH-_Rev"};  // GPIO18 | SPI CLK | PWM

// ------------ Fertilizer A ----------------
const pin_t pinPwmMot4Dose {19, "MOT_FERT_A_Dose"}; // GPIO19 | SPI MISO | PWM
const pin_t pinPwmMot4Rev  {13, "MOT_FERT_A_Rev"};  // GPIO13 | ADC2 | TOUCH | PWM

// ------------ Fertilizer B ----------------
const pin_t pinPwmMot5Dose {32, "MOT_FERT_B_Dose"}; // GPIO32 | ADC1 (RESERVED) | PWM
const pin_t pinPwmMot5Rev  {27, "MOT_FERT_B_Rev"};  // GPIO27 | ADC2_CH7 | PWM | SAFE

// ------------ Analog Sensors ----------------
const pin_t adcPh     {35, "PH"};        // GPIO35 | ADC1_CH7 | INPUT ONLY
const pin_t adcEc     {39, "EC"};        // GPIO39 | ADC1_CH3 | INPUT ONLY
const pin_t adcEcTemp {34, "EC_T"};      // GPIO34 | ADC1_CH6 | INPUT ONLY
const pin_t adcPhTemp {36, "PH_T"};      // GPIO36 | ADC1_CH0 | INPUT ONLY

// ------------ UNUSED / BOOT STRAP PINS ------------
const pin_t gpio0  {0,  "GPIO0"};    // BOOT MODE | STRAP PIN | AVOID
const pin_t gpio2  {2,  "GPIO2"};    // BOOT STRAP | ADC2_CH2 | AVOID
const pin_t gpio4  {4,  "GPIO4"};    // BOOT STRAP | ADC2_CH0 | TOUCH0 | AVOID
const pin_t gpio5  {5,  "GPIO5"};    // SPI CS | BOOT STRAP | AVOID
const pin_t gpio12 {12, "GPIO12"};   // BOOT STRAP | ADC2_CH5 | TOUCH5 | AVOID
const pin_t gpio15 {15, "GPIO15"};   // BOOT STRAP | ADC2_CH3 | AVOID      
const pin_t gpio14 {14, ""};         // GPIO14 | ADC2_CH6 | PWM | SAFE


// ---------------- GPIO ----------------
gpio_out_t led1{pinLed1};
gpio_out_t led2{pinLed2};

gpio_out_t gpioEcMeasure1{gpioEc1};
gpio_out_t gpioEcMeasure2{gpioEc2};

gpio_out_t gpioMainPump{gpioMot1};

pwm_out_t pwmPHplusDose(pinPwmMot2Dose, 1, 2000, 8, 0.8f, false);
pwm_out_t pwmPHplusRev(pinPwmMot2Rev, 2, 2000, 8, 0.8f, false);

pwm_out_t pwmPHminusDose(pinPwmMot3Dose, 3, 2000, 8, 0.8f, false);
pwm_out_t pwmPHminusRev(pinPwmMot3Rev, 4, 2000, 8, 0.8f, false);

pwm_out_t pwmFertilizerADose(pinPwmMot4Dose, 5, 2000, 8, 0.8f, false);
pwm_out_t pwmFertilizerARev(pinPwmMot4Rev, 6, 2000, 8, 0.8f, false);

pwm_out_t pwmFertilizerBDose(pinPwmMot5Dose, 7, 2000, 8, 0.8f, false);
pwm_out_t pwmFertilizerBRev(pinPwmMot5Rev, 8, 2000, 8, 0.8f, false);


// ---------------- Constants ----------------
const float VREF = 3.3;
const float TEMP0_K = 298.15;

const float R_SERIES_EC = 10000;
const float K_CELL = 1.0;
const float ALPHA = 0.02;

const float ph_p1 = 7.0;
const float ph_p2 = 4.0;

const float EC_A = 7.0;
const float EC_B = 4.0;

// ---------------- Calibration ----------------
cal_point_t ph_cal_1 {7.0, 2.2, 25.0};
cal_point_t ph_cal_2 {4.0, 2.86, 25.0};
cal_point_t ec_cal_1 {1.41, 2.50, 25.0};
cal_point_t ec_cal_2 {12.88, 3.00, 25.0};

const temp_cfg_t ec_temp_cfg {10000.0, 3950.0, 10000.0};
const temp_cfg_t ph_temp_cfg {10000.0, 3950.0, 10000.0};