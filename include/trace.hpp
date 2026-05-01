#include <Arduino.h>

// --- EC Print ---
void printEC(const pin_t &ec, float ecValue, float tempValue)
{
    Serial.print(ec.name);
    Serial.print(": ");
    Serial.print(ecValue);
    Serial.print(" uS/cm  ");
    Serial.print(tempValue);
    Serial.print("°C");
}

// --- PH Print ---
void printPH(const pin_t &ph, float phValue, float tempValue)
{
    Serial.print(ph.name);
    Serial.print(": ");
    Serial.print(phValue);
    Serial.print(" (raw)  ");
    Serial.print(tempValue);
    Serial.print("°C");
}