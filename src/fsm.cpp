#include "fsm.hpp"
#include "scheduler.hpp"
#include <settings.hpp>
#include "web.hpp"

// ---- external functions ----
extern void toggleLed1();
extern void disableLed1();
extern void toggleLed2();
extern void disableLed2();
extern void sensorProcess();
extern void sensorRead();

// ---- tasks ----
task_t led1_task            { "LED1" };
task_t led2_task            { "LED2" };
task_t t1_idle_task   { "WAIT" };
task_t t2_watering_task     { "WATERING" };
task_t t9_flush_task        { "FLUSH" };
task_t t3_measure_task      { "MEASURE" };
task_t t48_dose_task        { "DOSE" };
task_t measure_process_task { "MEAS_PROC" };
task_t measure_read_task    { "MEAS_READ" };

Time led1_timeout{Time::ms(500)};
Time led2_timeout{ Time::ms(100)};
Time t1_idle_timeout{Time::sec(10)};
Time t2_watering_timeout{Time::sec(10)};
Time t9_flush_timeout{Time::sec(5)};
Time t3_measure_timeout{Time::sec(3)};
Time t48_dose_timeout{Time::sec(3)};
Time measure_process_timeout{Time::us_(5000)};
Time measure_read_timeout{Time::us_(5000)};

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
  if (current) current->onEnter(*this);
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
    web_log(String("FSM: ") + from + " -> " + to);
    current->onEnter(*this);  
  }
}

// ==================================================
// STATES
// ==================================================

// ---------- INIT ----------

void InitState::enter(fsm_t &fsm) {
    auto &s = fsm.scheduler();
    s.addTask(led1_task, toggleLed1, led1_timeout, disableLed1 );
    s.addTask(led2_task, toggleLed2, led2_timeout, disableLed2);
    s.addTask(measure_process_task, sensorProcess,measure_process_timeout );
    s.addTask(measure_read_task, sensorRead, measure_read_timeout );
    s.addTask(t1_idle_task,     [&fsm]() {fsm.setDone();  }, t1_idle_timeout     );
    s.addTask(t2_watering_task, [&fsm]() {fsm.setDone();  }, t2_watering_timeout );
    s.addTask(t3_measure_task,  [&fsm]() {fsm.setDone();  }, t3_measure_timeout  );
    s.addTask(t48_dose_task,    [&fsm]() {fsm.setDone();  }, t48_dose_timeout    );
    s.addTask(t9_flush_task,    [&fsm]() {fsm.setDone();  }, t9_flush_timeout    );
}

void InitState::update(fsm_t &fsm) {
  fsm.transitionTo(&STATE_IDLE);
}


// ---------- IDLE ----------

void IdleState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.startTask(t1_idle_task);
}

void IdleState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(t1_idle_task);
}

void IdleState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if(done) {
    fsm.transitionTo(&STATE_WATERING);
  }
  {

  }
}


// ---------- WATERING ----------

void WateringState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.startTask(t2_watering_task);
  s.startTask(led1_task);
}

void WateringState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(t2_watering_task);
  s.stopTask(led1_task);
}

void WateringState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_MEASURE);
  }
  
}


// ---------- MEASURE ----------

void MeasureState::enter(fsm_t &fsm) 
{
  auto &s = fsm.scheduler();
  s.startTask(led2_task);
  s.startTask(measure_process_task);
  s.startTask(measure_read_task);
  s.startTask(t3_measure_task);
}

void MeasureState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(led2_task);
  s.stopTask(measure_process_task);
  s.stopTask(measure_read_task);
  s.stopTask(t3_measure_task);
}

void MeasureState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if (done)
  {
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
  s.startTask(t48_dose_task);
}

void RegulateState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(t48_dose_task);
}

void RegulateState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_FLUSH);
  }
}


// ---------- FLUSH ----------

void FlushState::enter(fsm_t &fsm) 
{
  auto &s = fsm.scheduler();
  s.startTask(t9_flush_task);
  s.startTask(led1_task);
}

void FlushState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(t9_flush_task);
  s.stopTask(led1_task);
}

void FlushState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
}


// ---------- CALIBRATE ----------

void CalibrateState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibrateState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibrateState::update(fsm_t &fsm) {
  if (fsm.error) {
    fsm.transitionTo(&STATE_ERROR);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
}


// ---------- ERROR ----------

void ErrorState::enter(fsm_t &fsm) {
}

void ErrorState::exit(fsm_t &fsm) {
}

void ErrorState::update(fsm_t &fsm) {
  if (fsm.error_ack) {
    fsm.transitionTo(&STATE_IDLE);
  }
}
