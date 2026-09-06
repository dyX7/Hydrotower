#pragma once
#include <Arduino.h>

// ==================================================
// GLOBAL TRACE COUNTER
// ==================================================
extern uint64_t _trace_id;

// ==================================================
// PIN DEFINITION
// ==================================================
struct pin_t
{
  int idx;
  const String name;
};

// ==================================================
// GPIO INPUT
// ==================================================
class gpio_in_t
{
public:

  gpio_in_t(const pin_t &pin, uint8_t mode = INPUT, bool trace = false)
  : p(pin), _trace(trace)
  {
    pinMode(p.idx, mode);
  }

  bool get() const
  {
    bool state = digitalRead(p.idx);

    if (_trace)
    {
      Serial.print(_trace_id++);
      Serial.print(" - ");
      Serial.print(p.name);
      Serial.println(state ? " ON" : " OFF");
    }

    return state;
  }

  const pin_t &p;

private:
  bool _trace;
};

// ==================================================
// GPIO OUTPUT
// ==================================================
class gpio_out_t
{
public:

  gpio_out_t(const pin_t &pin,
             bool default_state = false,
             bool trace = false)
  : p(pin), _trace(trace)
  {
    pinMode(p.idx, OUTPUT);
    set(default_state);
  }

  void set(bool state)
  {
    digitalWrite(p.idx, state);

    if(_trace)
    {
      Serial.print(_trace_id++);
      Serial.print(" - ");
      Serial.print(p.name);
      Serial.println(state ? " ON" : " OFF");
    }
  }

  const pin_t &p;

private:
  bool _trace;
};

// ==================================================
// PWM OUTPUT (ESP32 LEDC)
// ==================================================
class pwm_out_t
{
public:

  pwm_out_t(
      const pin_t &pin,
      uint8_t channel,
      uint32_t freq,
      uint8_t resolution,
      float duty,
      bool trace)
  : p(pin)
  , _channel(channel)
  , _freq(freq)
  , _resolution(resolution)
  , _trace(trace)
  {
    _defaultDuty = duty;
    _enabled = false;
    _percent = 0.0f;

    // Put GPIO into a known safe state first
    pinMode(p.idx, OUTPUT);
    digitalWrite(p.idx, LOW);

    // Configure PWM
    ledcSetup(_channel, _freq, _resolution);
    ledcAttachPin(p.idx, _channel);

    // IMPORTANT: explicitly force initial duty to zero
    ledcWrite(_channel, 0);
  }

  // ==================================================
  // ENABLE / DISABLE
  // ==================================================
  void set(bool enable)
  {
    _enabled = enable;

    if(_enabled)
      setDuty(_defaultDuty);
    else
      setDuty(0.0f);

    if(_trace)
    {
      Serial.print(_trace_id++);
      Serial.print(" - ");
      Serial.print(p.name);
      Serial.println(_enabled ? " PWM ON" : " PWM OFF");
    }
  }

  // ==================================================
  // DUTY CONTROL (0.0 - 1.0)
  // ==================================================
  void setDuty(float percent)
  {
    percent = constrain(percent, 0.0f, 1.0f);

    _percent = percent;

    uint32_t maxDuty = (1UL << _resolution) - 1;
    uint32_t duty = (uint32_t)(percent * maxDuty + 0.5f);

    ledcWrite(_channel, duty);
  }

  float getDuty() const
  {
    return _percent;
  }


  // ==================================================
  // FREQUENCY CONTROL
  // ==================================================
  void setFrequency(uint32_t freq)
  {
    _freq = freq;
    ledcSetup(_channel, _freq, _resolution);
  }

  uint32_t frequency() const
  {
    return _freq;
  }

  // ==================================================
  // RESOLUTION CONTROL
  // ==================================================
  void setResolution(uint8_t res)
  {
    _resolution = res;
    ledcSetup(_channel, _freq, _resolution);
    setDuty(_percent);
  }

  uint8_t resolution() const
  {
    return _resolution;
  }

  // ==================================================
  // RAW DUTY
  // ==================================================
  void setRaw(uint32_t duty)
  {
    uint32_t maxDuty = (1UL << _resolution) - 1;
    duty = min(duty, maxDuty);

    _percent = (float)duty / (float)maxDuty;
    ledcWrite(_channel, duty);
  }

  const pin_t &p;

private:
  uint8_t _channel;
  uint32_t _freq;
  uint8_t _resolution;

  float _percent = 0.0f;
  float _defaultDuty = 0.5f;

  bool _enabled = false;
  bool _trace;
};