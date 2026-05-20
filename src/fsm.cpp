#include "fsm.hpp"
#include "scheduler.hpp"
#include <settings.hpp>
#include "web.hpp"
#include <time.h>
#include <sensor.hpp>

// ---- external functions ----
extern void toggleLed1();
extern void disableLed1();
extern void toggleLed2();
extern void disableLed2();

inline Time minToTime(int minutes)
{
  return Time::sec(minutes * 60);
}

int cycle_time_minutes;
int watering_minutes;
int flush_minutes;
int fertilize_seconds;
int ph_seconds;

float ec_regulator;
float ec_tolerance;
float ph_regulator;
float ph_tolerance;

int sleep_start_hour;
int sleep_start_minute;
int sleep_end_hour;
int sleep_end_minute;

uint32_t remaining_seconds = 0;

// ---- tasks ----
task_t led1_task            { "LED1" };
task_t led2_task            { "LED2" };
task_t t1_idle_task         { "WAIT" };
task_t t21_watering_task    { "WATERING" };
task_t t22_watering_delay   { "WATERING_DELAY" };
task_t t9_flush_task        { "FLUSH" };
task_t t3_measure_task      { "MEASURE" };
task_t t45_fertilize_task   { "FERTILIZE" };
task_t t46_wait_task        { "REG_WAIT" };
task_t t67_ph_task          { "PH" };
task_t measure_process_task { "MEAS_PROC" };
task_t measure_read_task    { "MEAS_READ" };
task_t second_task          { "SECOND" };

Time led1_timeout{Time::ms(500)};
Time led2_timeout{ Time::ms(100)};
inline Time getCycleTime()        { return minToTime(cycle_time_minutes); }
inline Time getWateringTimeout()  { return minToTime(watering_minutes); }
inline Time getFlushTimeout()     { return minToTime(flush_minutes); }
inline Time getFertilizeTimeout() { return Time::sec(fertilize_seconds); }
inline Time getPhTimeout()        { return Time::sec(ph_seconds); }
inline bool fertilizerNeeded()    
{ 
  auto regUP = ec_regulator + ec_tolerance;
  auto regDOWN = ec_regulator - ec_tolerance;
  if(ecMeasure < regDOWN || ecMeasure > regUP)
  {
    web_log(String("Fertilizer needed:\n\tcurrent=") + String(ecMeasure, 2) + "\n\ttarget=" + String(ec_regulator, 2) + "\n\ttol=" + String(ec_tolerance, 2));
    return true;
  }
  else
  {
    return false;
  }
}

inline bool phPlusNeeded()        
{ auto reg = ph_regulator - ph_tolerance;
  if(phMeasure < reg)
  {
    web_log(String("PH+ needed:\n\tcurrent=") + String(phMeasure, 2) + "\n\ttarget=" + String(ph_regulator, 2) + "\n\ttol=" + String(ph_tolerance, 2));
    return true;
  }
  else
  {
    return false;
  }
}

inline bool phMinusNeeded()       
{ 
  auto reg = ph_regulator + ph_tolerance;
  if(phMeasure > reg)     
  {
    web_log(String("PH- needed:\n\tcurrent=") + String(phMeasure, 2) + "\n\ttarget=" + String(ph_regulator, 2) + "\n\ttol=" + String(ph_tolerance, 2));
    return true;
  }
  else
  {
    return false;
  }
}

Time t2_watering_timeout{getWateringTimeout()};
Time t9_flush_timeout{getFlushTimeout()};
Time t3_measure_timeout{Time::sec(3)};
Time t45_fertilize_timeout{getFertilizeTimeout()};
Time t46_wait_timeout{Time::sec(10)};
Time t67_ph_timeout{getPhTimeout()};
Time measure_process_timeout{Time::us_(5000)};
Time measure_read_timeout{Time::us_(5000)};

// ---------------- Instances ----------------

InitState STATE_INIT;

StopState STATE_STOP;
IdleState STATE_IDLE;

WateringPumpState STATE_WATERING_PUMP;
WateringWaitState STATE_WATERING_WAIT;
WateringState     STATE_WATERING{scheduler};

MeasureState STATE_MEASURE;

Regulate1_FertilizerAState  STATE_REG1_FERT_A;
Regulate2_WaitState         STATE_REG2_WAIT;
Regulate3_FertilizerBState  STATE_REG3_FERT_B;
RegulatePhState             STATE_REG_PH;
RegulateState               STATE_REGULATE{scheduler};

FlushState STATE_FLUSH;

CalibrateEcState STATE_CALIBRATE_EC;
CalibratePhState STATE_CALIBRATE_PH;


// ==================================================
// FSM helper
// ==================================================

const char* fsm_t::stateName() const {

  if (current == &STATE_INIT) return "INIT";
  if (current == &STATE_CALIBRATE_EC) return "CALIBRATE_EC";
  if (current == &STATE_CALIBRATE_PH) return "CALIBRATE_PH";

  if (current == &STATE_WATERING) return "WATERING";
  if (current == &STATE_WATERING_PUMP) return "WATERING_PUMP";
  if (current == &STATE_WATERING_WAIT) return "WATERING_WAIT";

  if (current == &STATE_MEASURE) return "MEASURE";

  if (current == &STATE_REGULATE) return "REGULATE";
  if (current == &STATE_REG1_FERT_A) return "REG_FERT_A";
  if (current == &STATE_REG2_WAIT) return "REG_WAIT";
  if (current == &STATE_REG3_FERT_B) return "REG_FERT_B";
  if (current == &STATE_REG_PH) return "REG_PH";


  if (current == &STATE_FLUSH) return "FLUSH";
  if (current == &STATE_STOP) return "STOP";
  if (current == &STATE_IDLE) return "IDLE";

  return "UNKNOWN";
}

bool fsm_t::isSleepTime()
{
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo))
  {
    return false;
  }

  int nowMinutes =
      timeinfo.tm_hour * 60 +
      timeinfo.tm_min;

  int startMinutes =
      sleep_start_hour * 60 +
      sleep_start_minute;

  int endMinutes =
      sleep_end_hour * 60 +
      sleep_end_minute;

  bool result;

  if(startMinutes > endMinutes)
  {
    result = (nowMinutes >= startMinutes ||
              nowMinutes < endMinutes);
  }
  else
  {
    result = (nowMinutes >= startMinutes &&
              nowMinutes < endMinutes);
  }

  return result;
}

void fsm_t::event1sec()
{
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo))
  {
    return;
  }

  int nowMinutes =
      timeinfo.tm_hour * 60 +
      timeinfo.tm_min;

  int startMinutes =
      sleep_start_hour * 60 +
      sleep_start_minute;

  int endMinutes =
      sleep_end_hour * 60 +
      sleep_end_minute;

  bool sleep = isSleepTime();

  // ---------------- STATE CHANGE TRACKING ----------------
  static bool lastSleep = false;

  if(sleep != lastSleep)
  {
    lastSleep = sleep;
  }

  // ---------------- FSM TRANSITION ----------------
  if(sleep)
  {
    if(current != &STATE_STOP)
    {
      web_log("FSM -> STOP (sleep active)");
      transitionTo(&STATE_STOP);
    }
  }
  else
  {
    if(current == &STATE_STOP && !error)
    {
      web_log("FSM -> IDLE (sleep ended)");
      transitionTo(&STATE_IDLE);
    }
  }
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
    s.addTask(measure_process_task, sensorProcess, measure_process_timeout );
    s.addTask(measure_read_task, sensorRead, measure_read_timeout );
    s.addTask(t1_idle_task,       [&fsm]() {fsm.setDone();}, getCycleTime()       );
    s.addTask(t21_watering_task,  [&fsm]() {STATE_WATERING_PUMP.done = true; }, getWateringTimeout() );
    s.addTask(t22_watering_delay, [&fsm]() {STATE_WATERING_WAIT.done = true; }, Time::sec(10) );
    s.addTask(t3_measure_task,    [&fsm]() {fsm.setDone();}, t3_measure_timeout   );
    s.addTask(t45_fertilize_task, []() { STATE_REG1_FERT_A.done = true; STATE_REG3_FERT_B.done = true;}, getFertilizeTimeout());
    s.addTask(t46_wait_task,      []() { STATE_REG2_WAIT.done = true; }, t46_wait_timeout );
    s.addTask(t67_ph_task,        []() { STATE_REG_PH.done = true; STATE_REG_PH.done = true;}, getPhTimeout());
    s.addTask(t9_flush_task,      [&fsm]() {fsm.setDone();}, getFlushTimeout()    );
    s.addTask(second_task,        [&fsm]() {fsm.event1sec();}, Time::sec(1)       );
    s.startTask(second_task);
}

void InitState::update(fsm_t &fsm) {
  fsm.transitionTo(&STATE_IDLE);
}

// ---------- STOP ----------
void StopState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.stopTask(t1_idle_task);

  remaining_seconds = 0;
  setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);
  setPump(pumps_t::FERTILIZER_A, pump_dir::STOP);
  setPump(pumps_t::FERTILIZER_B, pump_dir::STOP);
  setPump(pumps_t::PH_MINUS, pump_dir::STOP);
  setPump(pumps_t::PH_PLUS, pump_dir::STOP);
}

void StopState::exit(fsm_t &fsm) {
}

void StopState::update(fsm_t &fsm) {
}


// ---------- IDLE ----------

void IdleState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  s.setInterval(t1_idle_task, getCycleTime());
  s.startTask(t1_idle_task);
}

void IdleState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void IdleState::update(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  if (fsm.stop) {
    fsm.transitionTo(&STATE_STOP);
  }
  if(done) {
    fsm.transitionTo(&STATE_WATERING);
  }
  if (fsm.scheduler().nextSecond())
  {
    remaining_seconds = s.getRemainingTime(t1_idle_task);
  }
}


// ---------- WATERING ----------

void WateringState::enter(fsm_t &fsm)
{
    subFsm.begin(&STATE_WATERING_PUMP);
}

void WateringState::exit(fsm_t &fsm)
{
    if (subFsm.current)
    {
        subFsm.current->exit(subFsm);
    }
}

void WateringState::update(fsm_t &fsm)
{
    CompositeState::update(fsm);

    if (done)
    {
        fsm.transitionTo(&STATE_MEASURE);
    }
}

// ---------- WATERING PUMP ----------

void WateringPumpState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(t21_watering_task, getWateringTimeout());
    s.startTask(t21_watering_task);
    s.startTask(led1_task);
    setPump(pumps_t::MAIN_PUMP, pump_dir::PUMP);
}

void WateringPumpState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();
    s.stopTask(t21_watering_task);
    s.stopTask(led1_task);
    setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);
}

void WateringPumpState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (done)
    {
        fsm.transitionTo(&STATE_WATERING_WAIT);
    }
    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds = s.getRemainingTime(t21_watering_task);
    }
}

// ---------- WATERING WAIT ----------

void WateringWaitState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();
    s.setInterval(t22_watering_delay, Time::sec(10));
    s.startTask(t22_watering_delay);
}

void WateringWaitState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();
    s.stopTask(t22_watering_delay);
}

void WateringWaitState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (done)
    {
        fsm.transitionTo(&STATE_MEASURE);
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t22_watering_delay);
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
  auto &s = fsm.scheduler();
  if (fsm.stop) {
    fsm.transitionTo(&STATE_STOP);
  }
  if (done)
  {
    if (fertilizerNeeded() ||phPlusNeeded() || phMinusNeeded())
    {
      fsm.transitionTo(&STATE_REGULATE);
    } 
    else {
      fsm.transitionTo(&STATE_IDLE);
    }
  }
  if (fsm.scheduler().nextSecond())
  {
    remaining_seconds = s.getRemainingTime(t3_measure_task);
  }
}

// ---------- REGULATE ----------

void RegulateState::enter(fsm_t &fsm)
{
    setPump(pumps_t::MAIN_PUMP, pump_dir::PUMP);

    bool fert = fertilizerNeeded();
    bool phP  = phPlusNeeded();
    bool phM  = phMinusNeeded();

    if (fert)
    {
        subFsm.begin(&STATE_REG1_FERT_A);
    }
    else if (phP || phM)
    {
        STATE_REG_PH.mode = phP ? PhMode::PLUS : PhMode::MINUS;
        subFsm.begin(&STATE_REG_PH);
    }
    else
    {
        done = true;
    }
}

void RegulateState::exit(fsm_t &fsm)
{
    setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);
    setPump(pumps_t::FERTILIZER_A, pump_dir::STOP);
    setPump(pumps_t::FERTILIZER_B, pump_dir::STOP);
    setPump(pumps_t::PH_PLUS, pump_dir::STOP);
    setPump(pumps_t::PH_MINUS, pump_dir::STOP);

    if (subFsm.current)
        subFsm.current->exit(subFsm);
}

void RegulateState::update(fsm_t &fsm)
{
    CompositeState::update(fsm);

    if (done)
        fsm.transitionTo(&STATE_IDLE);
}


// ==================================================
// FERTILIZER A
// ==================================================

void Regulate1_FertilizerAState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(t45_fertilize_task, getFertilizeTimeout());
    s.startTask(t45_fertilize_task);

    setPump(pumps_t::FERTILIZER_A, pump_dir::PUMP);
}

void Regulate1_FertilizerAState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t45_fertilize_task);

    setPump(pumps_t::FERTILIZER_A, pump_dir::STOP);
}

void Regulate1_FertilizerAState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.scheduler().nextSecond())
        remaining_seconds = s.getRemainingTime(t45_fertilize_task);

    if (done)
        fsm.transitionTo(&STATE_REG2_WAIT);
}


// ==================================================
// WAIT
// ==================================================

void Regulate2_WaitState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(t46_wait_task, t46_wait_timeout);
    s.startTask(t46_wait_task);
}

void Regulate2_WaitState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t46_wait_task);
}

void Regulate2_WaitState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.scheduler().nextSecond())
        remaining_seconds = s.getRemainingTime(t46_wait_task);

    if (done)
        fsm.transitionTo(&STATE_REG3_FERT_B);
}


// ==================================================
// FERTILIZER B
// ==================================================

void Regulate3_FertilizerBState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(t45_fertilize_task, getFertilizeTimeout());
    s.startTask(t45_fertilize_task);

    setPump(pumps_t::FERTILIZER_B, pump_dir::PUMP);
}

void Regulate3_FertilizerBState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t45_fertilize_task);

    setPump(pumps_t::FERTILIZER_B, pump_dir::STOP);
}

void Regulate3_FertilizerBState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.scheduler().nextSecond())
        remaining_seconds = s.getRemainingTime(t45_fertilize_task);

    if (done)
    {
        if (phPlusNeeded())
        {
            STATE_REG_PH.mode = PhMode::PLUS;
            fsm.transitionTo(&STATE_REG_PH);
        }
        else if (phMinusNeeded())
        {
            STATE_REG_PH.mode = PhMode::MINUS;
            fsm.transitionTo(&STATE_REG_PH);
        }
        else
        {
            fsm.transitionTo(&STATE_FLUSH);
        }
    }
}


// ==================================================
// PH REGULATION (single combined state)
// ==================================================

void RegulatePhState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(t67_ph_task, getPhTimeout());
    s.startTask(t67_ph_task);

    if (mode == PhMode::PLUS)
        setPump(pumps_t::PH_PLUS, pump_dir::PUMP);
    else
        setPump(pumps_t::PH_MINUS, pump_dir::PUMP);
}

void RegulatePhState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t67_ph_task);

    setPump(pumps_t::PH_PLUS, pump_dir::STOP);
    setPump(pumps_t::PH_MINUS, pump_dir::STOP);
}

void RegulatePhState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.scheduler().nextSecond())
        remaining_seconds = s.getRemainingTime(t67_ph_task);

    if (done)
        fsm.transitionTo(&STATE_FLUSH);
}


// ---------- FLUSH ----------

void FlushState::enter(fsm_t &fsm) 
{
  setPump(pumps_t::MAIN_PUMP, pump_dir::PUMP);
  auto &s = fsm.scheduler();
  s.setInterval(t9_flush_task, getFlushTimeout());
  s.startTask(t9_flush_task);
  s.startTask(led1_task);
}

void FlushState::exit(fsm_t &fsm) {
  setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);
  auto &s = fsm.scheduler();
  s.stopTask(t9_flush_task);
  s.stopTask(led1_task);
}

void FlushState::update(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  if (fsm.stop) {
    fsm.transitionTo(&STATE_STOP);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
  if (fsm.scheduler().nextSecond())
  {
    remaining_seconds = s.getRemainingTime(t9_flush_task);
  }
}


// ---------- CALIBRATE EC ----------

void CalibrateEcState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibrateEcState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibrateEcState::update(fsm_t &fsm) {
  if (fsm.stop) {
    fsm.transitionTo(&STATE_STOP);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
}

// ---------- CALIBRATE PH ----------

void CalibratePhState::enter(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibratePhState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void CalibratePhState::update(fsm_t &fsm) {
  if (fsm.stop) {
    fsm.transitionTo(&STATE_STOP);
  }
  if (done)
  {
    fsm.transitionTo(&STATE_IDLE);
  }
}


void setPump(pumps_t pump, pump_dir dir)
{
    web_pumps(pump, dir);
    
    switch(pump)
    {
        case pumps_t::MAIN_PUMP:

            if(dir == pump_dir::STOP)
            {
                pwmMainPump.set(false);
            }
            else
            {
                pwmMainPump.set(true);
            }
            break;

        case pumps_t::PH_PLUS:
            if(dir == pump_dir::STOP)
            {
                pwmPHplusRev.set(false);
                pwmPHplusDose.set(false);
            
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmPHplusRev.set(false);
                pwmPHplusDose.set(true);
            }
            else
            {
                pwmPHplusDose.set(false);
                pwmPHplusRev.set(true);
            }
            break;

        case pumps_t::PH_MINUS:
            if(dir == pump_dir::STOP)
            {
                pwmPHminusRev.set(false);
                pwmPHminusDose.set(false);            
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmPHminusRev.set(false);
                pwmPHminusDose.set(true);
            }
            else
            {
                pwmPHminusDose.set(false);
                pwmPHminusRev.set(true);
            }
            break;

        case pumps_t::FERTILIZER_A:
            if(dir == pump_dir::STOP)
            {
                pwmFertilizerARev.set(false);
                pwmFertilizerADose.set(false);
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmFertilizerARev.set(false);
                pwmFertilizerADose.set(true);
            }
            else
            {
                pwmFertilizerADose.set(false);
                pwmFertilizerARev.set(true);
            }
            break;

        case pumps_t::FERTILIZER_B:
            if(dir == pump_dir::STOP)
            {
                pwmFertilizerBRev.set(false);
                pwmFertilizerBDose.set(false);
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmFertilizerBRev.set(false);
                pwmFertilizerBDose.set(true);
            }
            else
            {
                pwmFertilizerBDose.set(false);
                pwmFertilizerBRev.set(true);
            }
            break;
    }
}