#pragma once

#include <Arduino.h>
#include <scheduler.hpp>

class fsm_t;

// ---------------- Base State ----------------

class State {
public:
  void onEnter(fsm_t &fsm)
  {
    done = false;
    enter(fsm);
  }
  virtual void update(fsm_t &fsm) = 0;
  virtual void exit(fsm_t &fsm) {}
  virtual ~State() = default;
  
  virtual void enter(fsm_t &fsm) {}
  bool done = false;
};

// ---------------- FSM ----------------

class fsm_t {
public:
  fsm_t(scheduler_t &scheduler)
      : _s{scheduler} {}

  void begin(State *initial);
  void poll();
  void transitionTo(State *next);
  void setDone() {
    if (current) {
      current->done = true;
    }
  }

  scheduler_t &scheduler() { return _s; }

  // ---- flags ----
  bool deviation_detected = true;
  bool error = false;
  bool error_ack = false;

  bool idle_done = false;
  bool watering_done = false;
  bool measure_done = false;
  bool dose_done = false;
  bool flush_done = false;

  const char* stateName() const;

private:
  scheduler_t &_s;
  State *current = nullptr;
};

// ---------------- States ----------------

class InitState : public State {
public:
  void enter(fsm_t &fsm);
  void update(fsm_t &fsm) override;
};

class IdleState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class WateringState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class MeasureState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class RegulateState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class FlushState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class CalibrateState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;

private:
};

class ErrorState : public State {
public:
  void enter(fsm_t &fsm);
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