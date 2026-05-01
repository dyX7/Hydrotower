#pragma once

#include <Arduino.h>
#include <scheduler.hpp>

class fsm_t;

// ---------------- Base State ----------------

class State {
public:
  virtual void enter(fsm_t &fsm) {}
  virtual void update(fsm_t &fsm) = 0;
  virtual void exit(fsm_t &fsm) {}
  virtual ~State() = default;
};

// ---------------- FSM ----------------

class fsm_t {
public:
  fsm_t(scheduler_t &scheduler)
      : _s{scheduler} {}

  void begin(State *initial);
  void poll();
  void transitionTo(State *next);

  scheduler_t &scheduler() { return _s; }

  // ---- flags ----
  bool calib_done = false;
  bool watering_done = false;
  bool measurement_done = false;
  bool deviation_detected = false;
  bool regulation_done = false;
  bool flush_done = false;
  bool error = false;
  bool error_ack = false;

  bool force_idle = false;

  // ---- triggers ----
  void calibrated() { calib_done = true; }
  void watered() { watering_done = true; }

  const char* stateName() const;

private:
  scheduler_t &_s;
  State *current = nullptr;
};

// ---------------- States ----------------

class InitState : public State {
public:
  void update(fsm_t &fsm) override;
};

class CalibrateState : public State {
public:
  void enter(fsm_t &fsm) override;
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;

private:
};

class WateringState : public State {
public:
  void enter(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class MeasureState : public State {
public:
  void enter(fsm_t &fsm) override;
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class RegulateState : public State {
public:
  void enter(fsm_t &fsm) override;
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class FlushState : public State {
public:
  void enter(fsm_t &fsm) override;
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class ErrorState : public State {
public:
  void enter(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class IdleState : public State {
public:
  void enter(fsm_t &fsm) override;
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

// ---------------- Instances ----------------

extern InitState STATE_INIT;
extern CalibrateState STATE_CALIBRATE;
extern WateringState STATE_WATERING;
extern MeasureState STATE_MEASURE;
extern RegulateState STATE_REGULATE;
extern FlushState STATE_FLUSH;
extern ErrorState STATE_ERROR;
extern IdleState STATE_IDLE;