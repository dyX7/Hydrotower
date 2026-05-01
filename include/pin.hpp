#pragma once
#include <Arduino.h>

extern uint64_t _trace_id;

struct pin_t
{
  int idx; 
  const String name;
};

class gpio_out_t
{
  public: 
  gpio_out_t(const pin_t &pin, bool trace=false)
  : p{pin}
  , _trace{trace}
  {
    pinMode(p.idx, OUTPUT);
  }

  void set(bool state)
  {
    digitalWrite(p.idx, state);

    if(_trace)
    {
      Serial.print(_trace_id);
      Serial.print(" - ");
      Serial.print(p.name);

      if(state)
      {
        Serial.print(" on\n");
      }
      else
      {
        Serial.print(" off\n");
      }

      _trace_id++;
    }

  }

  const pin_t &p;
  bool _trace;
};


// ---------------- PWM CLASS ----------------
class pwm_out_t
{
public:
    pwm_out_t(const pin_t& pin, uint8_t channel, uint32_t freq = 5000, uint8_t resolution = 8)
        : _pin(pin), _channel(channel), _resolution(resolution)
    {
        ledcSetup(_channel, freq, _resolution);
        ledcAttachPin(_pin.idx, _channel);
    }

    void setDuty(float dutyPercent)
    {
        if(dutyPercent < 0) dutyPercent = 0;
        if(dutyPercent > 1) dutyPercent = 1;

        uint32_t maxDuty = (1 << _resolution) - 1;
        uint32_t duty = dutyPercent * maxDuty;

        ledcWrite(_channel, duty);
    }

private:
    pin_t _pin;
    uint8_t _channel;
    uint8_t _resolution;
};