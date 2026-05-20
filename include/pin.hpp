#pragma once
#include <Arduino.h>

extern uint64_t _trace_id;

struct pin_t
{
  int idx;
  const String name;
};

// ==================================================
// GPIO OUTPUT
// ==================================================
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
      Serial.print(_trace_id++);
      Serial.print(" - ");
      Serial.print(p.name);
      Serial.println(state ? " on" : " off");
    }
  }

  const pin_t &p;
  bool _trace;
};


// ==================================================
// PWM OUTPUT
// ==================================================
class pwm_out_t
{
public:

  pwm_out_t(
      const pin_t &pin,
      uint8_t channel,
      uint32_t freq = 20000,
      uint8_t resolution = 8,
      bool trace = false)
  : p(pin)
  , _channel(channel)
  , _freq(freq)
  , _resolution(resolution)
  , _trace(trace)
  {
    ledcSetup(_channel, _freq, _resolution);
    ledcAttachPin(p.idx, _channel);

    setDuty(0.0f);
  }

  // 0.0 -> 1.0
  void setDuty(float percent)
  {
    percent = constrain(percent, 0.0f, 1.0f);

    _percent = percent;

    uint32_t maxDuty = (1UL << _resolution) - 1;
    uint32_t duty = (uint32_t)(percent * maxDuty + 0.5f);

    ledcWrite(_channel, duty);

    if(_trace)
    {
      Serial.print(_trace_id++);
      Serial.print(" - ");
      Serial.print(p.name);
      Serial.print(" pwm=");
      Serial.println(percent, 3);
    }
  }

  // direct raw value
  void setRaw(uint32_t duty)
  {
    uint32_t maxDuty = (1UL << _resolution) - 1;

    if(duty > maxDuty)
      duty = maxDuty;

    ledcWrite(_channel, duty);

    _percent = (float)duty / (float)maxDuty;
  }

  void setFrequency(uint32_t freq)
  {
    _freq = freq;
    ledcSetup(_channel, _freq, _resolution);
  }

  float percent() const
  {
    return _percent;
  }

  uint32_t frequency() const
  {
    return _freq;
  }

  const pin_t &p;

private:

  uint8_t _channel;
  uint32_t _freq;
  uint8_t _resolution;

  float _percent = 0.0f;

  bool _trace;
};