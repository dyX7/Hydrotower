#pragma once

#include <Arduino.h>
#include <scheduler.hpp>

class fsm_t;
extern scheduler_t scheduler;

extern int cycle_time_minutes;
extern int watering_minutes;
extern int flush_minutes;
extern int fertilize_seconds;
extern int ph_seconds;

extern float ec_regulator;
extern float ec_tolerance;
extern float ph_regulator;
extern float ph_tolerance;

extern int sleep_start_hour;
extern int sleep_start_minute;
extern int sleep_end_hour;
extern int sleep_end_minute;

// ---------------- Base State ----------------

class State {
public:
  void onEnter(fsm_t &fsm)
  {
    done = false;
    enter(fsm);
  }
  virtual void update(fsm_t &fsm) = 0;
  virtual void exit(fsm_t &fsm)
  {
      done = false;
  }
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
  void setDone() { if (current) { current->done = true; } }
  void setStart() { stop  = false; error = false; }
  void setStop()  { stop = true; }
  void setError() { error = true; }
  bool isSleepTime();
  void event1sec();
  scheduler_t &scheduler() { return _s; }
  const char* stateName() const;

  bool stop = false;
  bool error = false;

  scheduler_t &_s;
  State *current = nullptr;
};

class CompositeState : public State {
public:
    CompositeState(scheduler_t& s)
        : subFsm{s} {}

    void update(fsm_t& fsm) override
    {
        subFsm.poll();

        // parent state finishes when child FSM finishes
        if (subFsm.current == nullptr) {
            done = true;
        }
    }
protected:
    fsm_t subFsm;
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

// --------------------------------------------------
// WATERING SUB STATES
// --------------------------------------------------

class WateringPumpState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class WateringWaitState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class WateringState : public CompositeState {
public:
    WateringState(scheduler_t& s)
        : CompositeState{s} {}

    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class MeasureState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

// --------------------------------------------------
// REGULATE SUB STATES
// ------------------------------------------------


class Regulate1_FertilizerAState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class Regulate2_WaitState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class Regulate3_FertilizerBState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

enum class PhMode {
    NONE,
    PLUS,
    MINUS
};
class RegulatePhState : public State {
public:
    PhMode mode = PhMode::NONE;

    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class RegulateState : public CompositeState {
public:
    RegulateState(scheduler_t& s)
        : CompositeState{s} {}

    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class FlushState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class CalibrateEcState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

class CalibratePhState : public State {
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
};

class StopState : public State {
public:
  void enter(fsm_t &fsm);
  void exit(fsm_t &fsm) override;
  void update(fsm_t &fsm) override;
};

void setPump(pumps_t pump, pump_dir dir);


// ---------------- Instances ----------------

extern InitState STATE_INIT;
extern StopState STATE_STOP;
extern IdleState STATE_IDLE;

extern WateringState STATE_WATERING;
extern MeasureState STATE_MEASURE;
extern RegulateState STATE_REGULATE;
extern FlushState STATE_FLUSH;
extern CalibrateEcState STATE_CALIBRATE_EC;
extern CalibratePhState STATE_CALIBRATE_PH;