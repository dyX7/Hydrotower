#include "fsm.hpp"
#include <settings.hpp>
#include "web.hpp"

// ---- external functions ----
extern void toggleLed1();
extern void toggleLed2();
extern void sensorProcess();
extern void sensorRead();

// ---- tasks ----
int led1_task;
int led2_task;
int t1_pump_task;
int t2_watering_task;
int t3_measure_task;
int t48_dose_task;
int measure_process_task;
int measure_read_task;

// ---------------- Instances ----------------

InitState STATE_INIT;
CalibrateState STATE_CALIBRATE;
WateringState STATE_WATERING;
MeasureState STATE_MEASURE;
RegulateState STATE_REGULATE;
FlushState STATE_FLUSH;
ErrorState STATE_ERROR;
IdleState STATE_IDLE;

// ==================================================
// FSM helper
// ==================================================

const char* fsm_t::stateName() const {
  if (current == &STATE_INIT) return "INIT";
  if (current == &STATE_CALIBRATE) return "CALIBRATE";
  if (current == &STATE_WATERING) return "WATERING";
  if (current == &STATE_MEASURE) return "MEASURE";
  if (current == &STATE_REGULATE) return "REGULATE";
  if (current == &STATE_FLUSH) return "FLUSH";
  if (current == &STATE_ERROR) return "ERROR";
  if (current == &STATE_IDLE) return "IDLE";
  return "UNKNOWN";
}

// ==================================================
// FSM CORE
// ==================================================

void fsm_t::begin(State *initial) {
  current = initial;
  if (current) current->enter(*this);
}

void fsm_t::poll() {
  if (current) current->update(*this);
}

void fsm_t::transitionTo(State *next) 
{
  const char* from = stateName();
  if (current)
  {
    current->exit(*this);
  }
  current = next;

  const char* to = stateName();
  if (current)
  {
    current->enter(*this);  
  }

  web_log(String("FSM: ") + from + " -> " + to);
}

// ==================================================
// STATES
// ==================================================

// ---------- INIT ----------

void InitState::update(fsm_t &fsm) {
  fsm.transitionTo(&STATE_IDLE);
}

// ---------- CALIBRATE ----------

void CalibrateState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.addTask(led2_task, toggleLed2, Time::ms(700));
}

void CalibrateState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.removeTask(led2_task);
}

void CalibrateState::update(fsm_t &fsm) {
  if (fsm.calib_done) {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- WATERING ----------

void WateringState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  fsm.watering_done = false;
  s.addTask(t2_watering_task, [&fsm]() {fsm.watering_done = true;}, Time::ms(20000));
}

void WateringState::update(fsm_t &fsm) {
  if (fsm.watering_done) {
    fsm.transitionTo(&STATE_MEASURE);
  }
}

// ---------- MEASURE ----------

void MeasureState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  fsm.measurement_done = false;
  s.addTask(measure_process_task, sensorProcess, Time::us_(5000));
  s.addTask(measure_read_task, sensorRead, Time::us_(5000));
  s.addTask(t3_measure_task, [&fsm]() {fsm.measurement_done = true; }, Time::ms(3000));
}

void MeasureState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.removeTask(measure_process_task);
  s.removeTask(measure_read_task);
  s.removeTask(t3_measure_task);
}

void MeasureState::update(fsm_t &fsm) {
  if (fsm.measurement_done) {
    if (fsm.deviation_detected) {
      fsm.transitionTo(&STATE_REGULATE);
    } else {
      fsm.transitionTo(&STATE_IDLE);
    }
  }
}

// ---------- REGULATE ----------

void RegulateState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  fsm.regulation_done = false;
  s.addTask(t48_dose_task, [&fsm]() {fsm.regulation_done = true;}, Time::ms(1000));
}

void RegulateState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.removeTask(t48_dose_task);
}

void RegulateState::update(fsm_t &fsm) {
  if (fsm.regulation_done) {
    fsm.transitionTo(&STATE_FLUSH);
  }
}

// ---------- FLUSH ----------

void FlushState::enter(fsm_t &fsm) 
{
  auto &s = fsm.scheduler();
  fsm.flush_done = false;
  s.addTask(t2_watering_task, [&fsm]() {fsm.flush_done = true;}, Time::ms(20000));
}

void FlushState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.removeTask(t2_watering_task);
}

void FlushState::update(fsm_t &fsm) {
  if (fsm.flush_done) {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- ERROR ----------

void ErrorState::enter(fsm_t &fsm) {
}

void ErrorState::update(fsm_t &fsm) {
  if (fsm.error_ack) {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- IDLE ----------

void IdleState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.addTask(led1_task, toggleLed1, Time::ms(500));
  s.addTask(t1_pump_task, [&fsm]() { fsm.transitionTo(&STATE_WATERING); }, Time::ms(5000));
}

void IdleState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.removeTask(led1_task);
  s.removeTask(t1_pump_task);
}

void IdleState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
}