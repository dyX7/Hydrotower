#pragma once

#include <Arduino.h>
#include <scheduler.hpp>

class fsm_t;

extern scheduler_t scheduler;

extern task_t t3_measure_task;
extern task_t measure_process_task;

extern int cycle_time_minutes;
extern int watering_minutes;
extern int flush_minutes;
extern int flush_delay_seconds;
extern int fertilize_seconds;
extern int ph_seconds;
extern int reverse_seconds;

extern float ec_regulator;
extern float ec_tolerance;
extern float ph_regulator;
extern float ph_tolerance;

extern bool stop_locked;
extern void setStopLock(bool lock);

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
  void eventCalib();
  scheduler_t &scheduler() { return _s; }
  const char* stateName() const;

  bool stop = false;
  bool error = false;

  scheduler_t &_s;
  State *current = nullptr;
};

class CompositeState : public State {
public:
    CompositeState(scheduler_t& s, State* activeState)
        : subFsm{s}, _activeState{activeState} {}

    void update(fsm_t& fsm) override
    {
        subFsm.poll();
        _activeState = subFsm.current;
        
        if (subFsm.current == nullptr) 
        {
            done = true;
        }
    }

    void exit(fsm_t& fsm) override
    {
        _activeState = nullptr;
    }

protected:
    fsm_t subFsm;
    State* _activeState;
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
    WateringState(scheduler_t& s, State* activeState)
        : CompositeState{s, activeState} {}

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
    RegulateState(scheduler_t& s, State* activeState)
        : CompositeState{s, activeState} {}

    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

// --------------------------------------------------
// FLUSH SUB STATES
// --------------------------------------------------

class FlushPumpState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class FlushWaitState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class FlushReverseState : public State {
public:
    void enter(fsm_t &fsm) override;
    void exit(fsm_t &fsm) override;
    void update(fsm_t &fsm) override;
};

class FlushState : public CompositeState {
public:
    FlushState(scheduler_t& s, State* activeState)
        : CompositeState{s, activeState} {}

    void enter(fsm_t &fsm) override;
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
void disableAllPumps();


// ---------------- Instances ----------------

extern InitState STATE_INIT;
extern StopState STATE_STOP;
extern IdleState STATE_IDLE;
extern CalibrateState STATE_CALIBRATE;

extern WateringPumpState STATE_WATERING_PUMP;
extern WateringWaitState STATE_WATERING_WAIT;
extern WateringState     STATE_WATERING;

extern MeasureState STATE_MEASURE;

extern Regulate1_FertilizerAState  STATE_REG1_FERT_A;
extern Regulate2_WaitState         STATE_REG2_WAIT;
extern Regulate3_FertilizerBState  STATE_REG3_FERT_B;
extern RegulatePhState             STATE_REG_PH;
extern RegulateState               STATE_REGULATE;

extern FlushPumpState STATE_FLUSH_PUMP;
extern FlushWaitState STATE_FLUSH_WAIT;
extern FlushReverseState STATE_FLUSH_REVERSE;
extern FlushState     STATE_FLUSH;