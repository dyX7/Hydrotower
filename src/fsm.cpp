#include "fsm.hpp"
#include <settings.hpp>

// ---- external functions from main.cpp ----
extern void toggleLed1();
extern void toggleLed2();
extern void sensorProcess();
extern void sensorRead();

// ---------------- Static Instances ----------------

InitState STATE_INIT;
CalibrateState STATE_CALIBRATE;
WateringState STATE_WATERING;
MeasureState STATE_MEASURE;
IdleState STATE_IDLE;

// ==================================================
// FSM helper
// ==================================================

const char* fsm_t::stateName() const {
  if (current == &STATE_INIT) return "INIT";
  if (current == &STATE_CALIBRATE) return "CALIBRATE";
  if (current == &STATE_WATERING) return "WATERING";
  if (current == &STATE_MEASURE) return "MEASURE";
  if (current == &STATE_IDLE) return "IDLE";
  return "UNKNOWN";
}

// ---------------- FSM CORE ----------------

void fsm_t::begin(State *initial) {
  current = initial;

  if (current) {
    Serial.print(_trace_id);
    Serial.print(" - State: ");
    Serial.println(stateName());

    current->enter(*this);
  }
}

void fsm_t::poll() {
  if (current) current->update(*this);
}

void fsm_t::transitionTo(State *next) {
  if (current) current->exit(*this);

  current = next;

  if (current) {
    Serial.print(_trace_id);
    Serial.print(" - State: ");
    Serial.println(stateName());

    current->enter(*this);
  }
}

// ==================================================
// ---------------- STATES ---------------------------
// ==================================================

// ---------- INIT ----------

void InitState::update(fsm_t &fsm) {
  // if (fsm.calib_done) {
    // fsm.transitionTo(&STATE_MEASURE);
  // } else {
  //   fsm.transitionTo(&STATE_CALIBRATE);
  // }
}

// ---------- CALIBRATE ----------

void CalibrateState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  led1Task = s.addTask(toggleLed1, Time::ms(500));
  led2Task = s.addTask(toggleLed2, Time::ms(700));
}

void CalibrateState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  if (led1Task != -1) s.removeTask(led1Task);
  if (led2Task != -1) s.removeTask(led2Task);
}

void CalibrateState::update(fsm_t &fsm) {
  if (fsm.calib_done) {
    fsm.transitionTo(&STATE_WATERING);
  }
}

// ---------- WATERING ----------

void WateringState::enter(fsm_t &fsm) {
  fsm.watering_done = false;
}

void WateringState::update(fsm_t &fsm)
{
  if (fsm.force_watering)
  {
    fsm.force_watering = false;
  }
  else if (fsm.force_idle)
  {
    fsm.force_idle = false;
  }

  if (fsm.watering_done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- MEASURE ----------

void MeasureState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  processTask = s.addTask(sensorProcess, Time::us_(5000));
  sensorTask  = s.addTask(sensorRead, Time::us_(5000));

  triggerTask = s.addTask([&fsm]() {
    fsm.measured();
  }, Time::ms(500));
}

void MeasureState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();

  if (processTask != -1) s.removeTask(processTask);
  if (sensorTask != -1)  s.removeTask(sensorTask);
  if (triggerTask != -1)  s.removeTask(triggerTask);
}

void MeasureState::update(fsm_t &fsm) {
  if (fsm.measurement_done) {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- IDLE ----------

void IdleState::enter(fsm_t &fsm) {}

void IdleState::update(fsm_t &fsm) {
  static uint32_t last = 0;

  if (millis() - last > 5000) {
    last = millis();
    // fsm.transitionTo(&STATE_WATERING);
  }
}