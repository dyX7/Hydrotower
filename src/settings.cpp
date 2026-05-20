#include "settings.hpp"

// ------------ GPIO Pins ----------------
const pin_t pinLed2     {23, "LED2"};       // SPI MOSI | PWM | SAFE GPIO
const pin_t pinLed1     {22, "LED1"};       // I2C SCL | PWM | SAFE GPIO

const pin_t gpioEc1     {25, "EC1_PWM"};    // DAC1 | ADC2_CH8 | PWM
const pin_t gpioEc2     {26, "EC2_PWM"};    // DAC2 | ADC2_CH9 | PWM
const pin_t gpioPh1     {27, "PH1_PWM"};    // ADC2_CH7 | TOUCH7 | PWM
const pin_t gpioPh2     {14, "PH2_PWM"};    // ADC2_CH6 | TOUCH6 | PWM

const pin_t pinPwmMot1      {21, "MOT_MAIN"};       // I2C SDA | PWM
const pin_t pinPwmMot2Dose  {13, "MOT_PH+_Dose"};   // ADC2_CH4 | TOUCH4 | PWM
const pin_t pinPwmMot2Rev   {16, "MOT_PH+_Rev"};    // UART2 RX | PWM
const pin_t pinPwmMot3Dose  {9,  "MOT_PH-_Dose"};   // FLASH PIN (DO NOT USE)
const pin_t pinPwmMot3Rev   {17, "MOT_PH-_Rev"};    // UART2 TX | PWM
const pin_t pinPwmMot4Dose  {10, "MOT_FERTILZER_A_Dose"};   // FLASH PIN (DO NOT USE)
const pin_t pinPwmMot4Rev   {18, "MOT_FERTILZER_A_Rev"};    // SPI CLK | PWM
const pin_t pinPwmMot5Dose  {22, "MOT_FERTILZER_B_Dose"};   // I2C SCL | PWM

const pin_t pinPwmMot5Rev   {19, "MOT_FERTILZER_B_Rev"};    // SPI MISO | PWM

// ----------- Analog Pins ----------------
const pin_t adcEc       {36, "EC"};          // ADC1_CH0 | INPUT ONLY
const pin_t adcPh       {39, "PH"};          // ADC1_CH3 | INPUT ONLY
const pin_t adcEcTemp   {34, "EC_T"};        // ADC1_CH6 | INPUT ONLY
const pin_t adcPhTemp   {35, "PH_T"};        // ADC1_CH7 | INPUT ONLY

// ==================================================
// ADDITIONAL ESP32-WROOM-32 SAFE GPIOs
// ==================================================
// ------------ Avoid Pins ----------------
// GPIO 1 (TX0)
// GPIO 3 (RX0)
// GPIO 0 (BOOT)
// GPIO 2 (BOOT)
// GPIO 4 (BOOT)
// GPIO 5 (BOOT)
// GPIO 12 (BOOT)
// GPIO 15 (BOOT)

// const pin_t gpio1   {1,  "TX0"};     // UART TX | DEBUG
// const pin_t gpio3   {3,  "RX0"};     // UART RX | DEBUG
// const pin_t gpio0   {0,  "GPIO0"};   // BOOT STRAP | AVOID HIGH AT BOOT
// const pin_t gpio2   {2,  "GPIO2"};   // BOOT STRAP | ADC2_CH2 | PWM
// const pin_t gpio4   {4,  "GPIO4"};   // ADC2_CH0 | TOUCH0 | PWM
// const pin_t gpio5   {5,  "GPIO5"};   // SPI CS | PWM
// const pin_t gpio12  {12, "GPIO12"};  // ADC2_CH5 | TOUCH5 | BOOT STRAP | PWM
// const pin_t gpio15  {15, "GPIO15"};  // BOOT STRAP | ADC2_CH3 | PWM

const pin_t gpio23  {23, "GPIO23"};  // SPI MOSI | PWM
const pin_t gpio32  {32, "GPIO32"};  // ADC1_CH4 | TOUCH9 | PWM
const pin_t gpio33  {33, "GPIO33"};  // ADC1_CH5 | TOUCH8 | PWM

// ---------------- GPIO ----------------
gpio_out_t led1{pinLed1};
gpio_out_t led2{pinLed2};

gpio_out_t gpioEcMeasure1{gpioEc1};
gpio_out_t gpioEcMeasure2{gpioEc2};
gpio_out_t gpioPhMeasure1{gpioPh1};
gpio_out_t gpioPhMeasure2{gpioPh2};

gpio_out_t pwmMainPump{pinPwmMot1};
gpio_out_t pwmPHplusDose{pinPwmMot2Dose};
gpio_out_t pwmPHplusRev{pinPwmMot2Rev};
gpio_out_t pwmPHminusDose{pinPwmMot3Dose};
gpio_out_t pwmPHminusRev{pinPwmMot3Rev};
gpio_out_t pwmFertilizerADose{pinPwmMot4Dose};
gpio_out_t pwmFertilizerARev{pinPwmMot4Rev};
gpio_out_t pwmFertilizerBDose{pinPwmMot5Dose};
gpio_out_t pwmFertilizerBRev{pinPwmMot5Rev};


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
cal_point_t ph_cal_1 {7.0, 2.50, 25.0};
cal_point_t ph_cal_2 {4.0, 3.00, 25.0};
cal_point_t ec_cal_1 {1.41, 2.50, 25.0};
cal_point_t ec_cal_2 {12.88, 3.00, 25.0};

const temp_cfg_t ec_temp_cfg {10000.0, 3950.0, 10000.0};
const temp_cfg_t ph_temp_cfg {10000.0, 3950.0, 10000.0};

const int CTRL_FREQ = 5000;
const float CTRL_TIME = 1000000/(CTRL_FREQ*2*3);
const float MEASURE_TIME = 500;

// ---------------- IMAGES ----------------

const char img_idle[] PROGMEM =
R"rawliteral(iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGNgYAAAAAMAASsJTYQAAAAASUVORK5CYII=)rawliteral";
