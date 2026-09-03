#include "fsm.hpp"
#include "scheduler.hpp"
#include <settings.hpp>
#include "web.hpp"
#include <time.h>
#include <sensor.hpp>

// ---- external functions ----

inline Time minToTime(int minutes)
{
  return Time::sec(minutes * 60);
}

State* activeSubState = nullptr;

int cycle_time_minutes;
int watering_minutes;
int flush_minutes;
int flush_delay_seconds = 20;
int reverse_seconds = 10;
int fertilize_seconds;
int ph_seconds;

float ec_regulator;
float ec_tolerance;
float ph_regulator;
float ph_tolerance;

bool added_fertilizer{false};
bool added_ph_plus{false};
bool added_ph_minus{false};

bool sensor_process{false};

const Time FERTILIZER_LOCK  =  Time::hours(12);
const Time PH_PLUS_LOCK     =  Time::hours(48);
const Time PH_MINUS_LOCK    =  Time::hours(24);

uint32_t firtilizer_unlock{100};
uint32_t ph_plus_unlock{100};
uint32_t ph_minus_unlock{100};

uint32_t fertilize_cycle = 0;
uint32_t ph_plus_cycle = 0;
uint32_t ph_minus_cycle = 0;

bool stop_locked{false};
void setStopLock(bool lock)
{
    web_log(String("Stop Lock ") + String(lock));
    stop_locked = lock;
}

int sleep_start_hour;
int sleep_start_minute;
int sleep_end_hour;
int sleep_end_minute;

uint32_t remaining_seconds = 0;

task_t t1_idle_task         { "WAIT" };
task_t t21_watering_task    { "WATERING" };
task_t t22_watering_delay   { "WATERING_DELAY" };
task_t t8_flush_task        { "FLUSH" };
task_t t9_flush_delay_task  { "FLUSH_DELAY" };
task_t t10_reverse_task     { "REVERSE" };
task_t t3_measure_ec_task   { "MEASURE_EC" };
task_t t3_measure_ph_task   { "MEASURE_PH" };
task_t t4_calibrate_ec_task { "CALIBRATE_EC" };
task_t t5_calibrate_ph_task { "CALIBRATE_PH" };
task_t t45_fertilize_task   { "FERTILIZE" };
task_t t46_wait_task        { "REG_WAIT" };
task_t t67_ph_task          { "PH" };
task_t second_task          { "SECOND" };
task_t sensor_proc_task     { "SENSOR_PROC" };

inline Time getCycleTime()        { return minToTime(cycle_time_minutes); }
inline Time getWateringTimeout()  { return minToTime(watering_minutes); }
inline Time getFlushTimeout()     { return minToTime(flush_minutes); }
inline Time getFlushDelayTimeout(){ return Time::sec(flush_delay_seconds); }
inline Time getFertilizeTimeout() { return Time::sec(fertilize_seconds); }
inline Time getPhTimeout()        { return Time::sec(ph_seconds); }
inline Time getReverseTimeout()   { return Time::sec(reverse_seconds); }
inline bool fertilizerNeeded()
{ 
    if(fertilize_cycle > firtilizer_unlock)
    {
        if(ecMeasure < (ec_regulator - ec_tolerance))
        {
            web_log(String("Fertilizer needed:\n\tcurrent=") + String(ecMeasure, 2) + "\n\ttarget=" + String(ec_regulator, 2) + "\n\ttol=" + String(ec_tolerance, 2));
            return true;
        }
    }
    return false;

}

inline bool phPlusNeeded()        
{ 
    if(ph_plus_cycle > ph_plus_unlock)
    {
        if(phMeasure < (ph_regulator - ph_tolerance))
        {
            web_log(String("PH+ needed:\n\tcurrent=") + String(phMeasure, 2) + "\n\ttarget=" + String(ph_regulator, 2) + "\n\ttol=" + String(ph_tolerance, 2));
            return true;
        }
    }
    return false;
}

inline bool phMinusNeeded()       
{ 
    if(ph_minus_cycle > ph_minus_unlock)
    {
        if(phMeasure > (ph_regulator + ph_tolerance))     
        {
          web_log(String("PH- needed:\n\tcurrent=") + String(phMeasure, 2) + "\n\ttarget=" + String(ph_regulator, 2) + "\n\ttol=" + String(ph_tolerance, 2));
          return true;
        }
    }
    return false;
}

Time t2_watering_timeout{getWateringTimeout()};
Time t8_flush_timeout{getFlushTimeout()};
Time t9_flush_delay_timeout{getFlushDelayTimeout()};
Time t3_measure_ec_timeout{Time::sec(3)};
Time t3_measure_ph_timeout{Time::sec(3)};
Time t4_calibrate_ec_timeout{Time::sec(3)};
Time t5_calibrate_ph_timeout{Time::sec(3)};
Time t45_fertilize_timeout{getFertilizeTimeout()};
Time t46_wait_timeout{Time::sec(10)};
Time t67_ph_timeout{getPhTimeout()};

// ---------------- Instances ----------------

InitState STATE_INIT;

StopState STATE_STOP;
IdleState STATE_IDLE;

WateringPumpState STATE_WATERING_PUMP;
WateringWaitState STATE_WATERING_WAIT;
WateringState     STATE_WATERING{scheduler, activeSubState};

MeasureEcState STATE_MEASURE_EC;
MeasurePhState STATE_MEASURE_PH;
MeasureState   STATE_MEASURE{scheduler, activeSubState};

Regulate1_FertilizerAState  STATE_REG1_FERT_A;
Regulate2_WaitState         STATE_REG2_WAIT;
Regulate3_FertilizerBState  STATE_REG3_FERT_B;
RegulatePhState             STATE_REG_PH;
RegulateState               STATE_REGULATE{scheduler, activeSubState};

FlushState     STATE_FLUSH{scheduler, activeSubState};
FlushPumpState STATE_FLUSH_PUMP;
FlushWaitState STATE_FLUSH_WAIT;
FlushReverseState STATE_FLUSH_REVERSE;

CalibrateEcState STATE_CALIBRATE_EC;
CalibratePhState STATE_CALIBRATE_PH;


// ==================================================
// FSM helper
// ==================================================

const char* fsm_t::stateName() const
{
  // ---------------- TOP LEVEL ----------------
  if (current == &STATE_INIT) return "INIT";
  if (current == &STATE_STOP) return "STOP";
  if (current == &STATE_IDLE) return "IDLE";

  if (current == &STATE_CALIBRATE_EC) return "CALIBRATE_EC";
    if (current == &STATE_CALIBRATE_PH) return "CALIBRATE_PH";
  // ---------------- SUB STATES (MEASURE) ----------------
  if (current == &STATE_MEASURE) return "MEASURE";
  if (current == &STATE_MEASURE_EC) return "MEASURE_EC";
  if (current == &STATE_MEASURE_PH) return "MEASURE_PH";
  
  // ---------------- SUB STATES (FLUSH) ----------------
  if (current == &STATE_FLUSH) return "FLUSH";
  if (current == &STATE_FLUSH_PUMP) return "FLUSH_PUMP";
  if (current == &STATE_FLUSH_WAIT) return "FLUSH_WAIT";
  if (current == &STATE_FLUSH_REVERSE) return "FLUSH_REVERSE";
  
  // ---------------- SUB STATES (WATERING) ----------------
  if (current == &STATE_WATERING) return "WATERING";
  if (current == &STATE_WATERING_PUMP) return "WATERING_PUMP";
  if (current == &STATE_WATERING_WAIT) return "WATERING_WAIT";

  // ---------------- SUB STATES (REGULATE) ----------------
  if (current == &STATE_REGULATE) return "REGULATE";
  if (current == &STATE_REG1_FERT_A) return "FERT_A";
  if (current == &STATE_REG2_WAIT) return "REG_WAIT";
  if (current == &STATE_REG3_FERT_B) return "FERT_B";
  if (current == &STATE_REG_PH)
  {
    if (STATE_REG_PH.mode == PhMode::PLUS) return "PH_PLUS";
    if (STATE_REG_PH.mode == PhMode::MINUS) return "PH_MINUS";
    return "PH";
  }

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

void fsm_t::setSensorProcess()
{
    sensor_process = true;
}

void fsm_t::event1sec()
{
  struct tm timeinfo;

  tempProcess();

  if(!getLocalTime(&timeinfo))
  {
    return;
  }

  if(!stop_locked)
  {
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
        web_log("FSM -> STOP (SLEEP)");
        transitionTo(&STATE_STOP);
      }
    }
    else
    {
      if(current == &STATE_STOP && !error)
      {
        web_log("FSM -> IDLE (WAKE)");
        transitionTo(&STATE_IDLE);
      }
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
    s.addTask(t1_idle_task,       [&fsm]() {fsm.setDone();}, getCycleTime() );
    s.addTask(t21_watering_task,  [&fsm]() {STATE_WATERING_PUMP.done = true; }, getWateringTimeout() );
    s.addTask(t22_watering_delay, [&fsm]() {STATE_WATERING_WAIT.done = true; }, Time::sec(10) );
    s.addTask(t3_measure_ec_task, [&fsm]() {fsm.setDone();}, t3_measure_ec_timeout   );
    s.addTask(t3_measure_ph_task, [&fsm]() {fsm.setDone();}, t3_measure_ph_timeout   );
    s.addTask(t4_calibrate_ec_task, [&fsm]() {fsm.setDone();}, t4_calibrate_ec_timeout );
    s.addTask(t5_calibrate_ph_task, [&fsm]() {fsm.setDone();}, t5_calibrate_ph_timeout );
    s.addTask(t45_fertilize_task, []() { STATE_REG1_FERT_A.done = true; STATE_REG3_FERT_B.done = true;}, getFertilizeTimeout());
    s.addTask(t46_wait_task,      []() { STATE_REG2_WAIT.done = true; }, t46_wait_timeout );
    s.addTask(t67_ph_task,        []() { STATE_REG_PH.done = true; STATE_REG_PH.done = true;}, getPhTimeout());
    s.addTask(t8_flush_task,      []() { STATE_FLUSH_PUMP.done = true; }, getFlushTimeout() );
    s.addTask(t9_flush_delay_task,[]() { STATE_FLUSH_WAIT.done = true; }, getFlushDelayTimeout() );
    s.addTask(t10_reverse_task,   []() { STATE_FLUSH_REVERSE.done = true; }, getReverseTimeout() );
    s.addTask(second_task,        [&fsm]() {fsm.event1sec();}, Time::sec(1)       );
    s.addTask(sensor_proc_task,   [&fsm]() {fsm.setSensorProcess(); }, Time::ms(20) );
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
  disableAllPumps();
  gpioEnDevices.set(false);
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
  disableEc();
  disableAllPumps();
  gpioEnDevices.set(false);
}

void IdleState::exit(fsm_t &fsm) {
  auto &s = fsm.scheduler();
}

void IdleState::update(fsm_t &fsm) {
  auto &s = fsm.scheduler();
  if (fsm.stop) {
    setStopLock(true);
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
    setPump(pumps_t::MAIN_PUMP, pump_dir::PUMP);
}

void WateringPumpState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();
    s.stopTask(t21_watering_task);
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

// ==================================================
// MEASURE EC
// ==================================================

void MeasureEcState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    done = false;

    s.setInterval(
        t3_measure_ec_task,
        t3_measure_ec_timeout);

    s.startTask(t3_measure_ec_task);
    s.startTask(sensor_proc_task);
    enableEc();
}


void MeasureEcState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t3_measure_ec_task);
    s.stopTask(sensor_proc_task);
    disableEc();
    web_log("ec=" + String(ecMeasure, 2) + "vEc=" + String(vEcFilter.get(), 3));
}


void MeasureEcState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.stop)
    {
        setStopLock(true);
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (done)
    {
        fsm.transitionTo(&STATE_MEASURE_PH);
        return;
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t3_measure_ec_task);
    }
}

// ==================================================
// MEASURE PH
// ==================================================

void MeasurePhState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    done = false;

    s.setInterval(
        t3_measure_ph_task,
        t3_measure_ph_timeout);

    s.startTask(t3_measure_ph_task);
    s.startTask(sensor_proc_task);
    enablePh();
}


void MeasurePhState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t3_measure_ph_task);
    s.stopTask(sensor_proc_task);
    disablePh();
    web_log("ph=" + String(phMeasure, 2) + "vPh=" + String(vPhFilter.get(), 3));
}


void MeasurePhState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.stop)
    {
        setStopLock(true);
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (done)
    {
        // End of measurement sub-FSM
        fsm.current = nullptr;
        return;
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t3_measure_ph_task);
    }
}

// ==================================================
// MEASURE COMPOSITE STATE
// ==================================================

void MeasureState::enter(fsm_t &fsm)
{
    done = false;

    // Start with EC measurement
    subFsm.begin(&STATE_MEASURE_EC);
}


void MeasureState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (subFsm.current)
    {
        subFsm.current->exit(subFsm);
    }

    // Make sure both measurement timers are stopped
    s.stopTask(t3_measure_ec_task);
    s.stopTask(t3_measure_ph_task);

    disableEc();

    // Measurement sequence completely finished
    s.stopTask(sensor_proc_task);
}


void MeasureState::update(fsm_t &fsm)
{
    // Run EC -> PH sub-state machine
    CompositeState::update(fsm);

    if (fsm.stop)
    {
        setStopLock(true);
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    // Both EC and PH measurements are complete
    if (done)
    {
        fertilize_cycle++;
        ph_plus_cycle++;
        ph_minus_cycle++;

        firtilizer_unlock =
            FERTILIZER_LOCK.us /
            Time::min(cycle_time_minutes).us;

        ph_plus_unlock =
            PH_PLUS_LOCK.us /
            Time::min(cycle_time_minutes).us;

        ph_minus_unlock =
            PH_MINUS_LOCK.us /
            Time::min(cycle_time_minutes).us;

        web_log(
            "fertilize cycle " +
            String(fertilize_cycle) +
            "/" +
            String(firtilizer_unlock));

        web_log(
            "ph_plus_cycle " +
            String(ph_plus_cycle) +
            "/" +
            String(ph_plus_unlock));

        web_log(
            "ph_minus_cycle " +
            String(ph_minus_cycle) +
            "/" +
            String(ph_minus_unlock));

        // Both EC and PH are now updated
        web_add_data_hist(
            ecMeasure,
            phMeasure,
            tempMeasure);

        if (calibration_valid() &&
            (fertilizerNeeded() ||
             phPlusNeeded() ||
             phMinusNeeded()))
        {
            fsm.transitionTo(&STATE_REGULATE);
        }
        else
        {
            fsm.transitionTo(&STATE_IDLE);
        }
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
    disableAllPumps();

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
    added_fertilizer = true;
    fertilize_cycle = 0;

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
    
    ph_plus_cycle = 0;
    ph_minus_cycle = 0;

    if (mode == PhMode::PLUS)
    {
        added_ph_plus = true; 
        setPump(pumps_t::PH_PLUS, pump_dir::PUMP);
    }
    else
    {
        added_ph_minus = true;
        setPump(pumps_t::PH_MINUS, pump_dir::PUMP);
    }
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
    subFsm.begin(&STATE_FLUSH_PUMP);
}

void FlushState::exit(fsm_t &fsm)
{
    if (subFsm.current)
    {
        subFsm.current->exit(subFsm);
    }
}

void FlushState::update(fsm_t &fsm)
{
    CompositeState::update(fsm);

    if (done)
    {
        fsm.transitionTo(&STATE_IDLE);
    }
}

void FlushPumpState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    setPump(pumps_t::MAIN_PUMP, pump_dir::PUMP);

    s.setInterval(t8_flush_task, getFlushTimeout());
    s.startTask(t8_flush_task);
}

void FlushPumpState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);

    s.stopTask(t8_flush_task);
}

void FlushPumpState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (done)
    {
        fsm.transitionTo(&STATE_FLUSH_WAIT);
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t8_flush_task);
    }
}

void FlushWaitState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(
        t9_flush_delay_task,
        getFlushDelayTimeout());

    s.startTask(t9_flush_delay_task);
}

void FlushWaitState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t9_flush_delay_task);
}

void FlushWaitState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (done)
    {
        fsm.transitionTo(&STATE_FLUSH_REVERSE);
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t9_flush_delay_task);
    }
}

void FlushReverseState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.setInterval(
        t10_reverse_task,
        getReverseTimeout());

    s.startTask(t10_reverse_task);

    // reverse pumps
    if(added_fertilizer)
    {
        setPump(pumps_t::FERTILIZER_A, pump_dir::REVERSE);
        setPump(pumps_t::FERTILIZER_B, pump_dir::REVERSE);
    }
    if(added_ph_minus)
    {
        setPump(pumps_t::PH_MINUS, pump_dir::REVERSE);
    }
    if(added_ph_plus)
    {
        setPump(pumps_t::PH_PLUS, pump_dir::REVERSE);
    }
}

void FlushReverseState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    added_fertilizer = false;
    added_ph_minus = false;
    added_ph_plus = false;

    disableAllPumps();

    s.stopTask(t10_reverse_task);
}

void FlushReverseState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (done)
    {
        fsm.current = nullptr;
        return;
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t10_reverse_task);
    }
}


// ==================================================
// CALIBRATE EC
// ==================================================

void CalibrateEcState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    done = false;

    s.setInterval( t4_calibrate_ec_task,
        t4_calibrate_ec_timeout);

    s.startTask(t4_calibrate_ec_task);
    s.startTask(sensor_proc_task);
    enableEc();
}

void CalibrateEcState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    s.stopTask(t4_calibrate_ec_task);
    s.stopTask(sensor_proc_task);
    disableEc();
    setActiveCalibration(point_t::DISBALED);
}

void CalibrateEcState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.stop)
    {
        setStopLock(true);
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (done)
    {
        applyCalibration();
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t4_calibrate_ec_task);
    }
}

// ==================================================
// CALIBRATE PH
// ==================================================

void CalibratePhState::enter(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    done = false;

    s.setInterval(
        t5_calibrate_ph_task,
        t5_calibrate_ph_timeout);

    s.startTask(t5_calibrate_ph_task);
    s.startTask(sensor_proc_task);
}

void CalibratePhState::exit(fsm_t &fsm)
{
    auto &s = fsm.scheduler();
    s.stopTask(t5_calibrate_ph_task);
    s.stopTask(sensor_proc_task);
    setActiveCalibration(point_t::DISBALED);
}

void CalibratePhState::update(fsm_t &fsm)
{
    auto &s = fsm.scheduler();

    if (fsm.stop)
    {
        setStopLock(true);
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (done)
    {
        applyCalibration();
        fsm.transitionTo(&STATE_STOP);
        return;
    }

    if (fsm.scheduler().nextSecond())
    {
        remaining_seconds =
            s.getRemainingTime(t5_calibrate_ph_task);
    }
}


void disableAllPumps()
{
    setPump(pumps_t::MAIN_PUMP, pump_dir::STOP);
    setPump(pumps_t::FERTILIZER_A, pump_dir::STOP);
    setPump(pumps_t::FERTILIZER_B, pump_dir::STOP);
    setPump(pumps_t::PH_PLUS, pump_dir::STOP);
    setPump(pumps_t::PH_MINUS, pump_dir::STOP);
}

void setPump(pumps_t pump, pump_dir dir)
{   
    float duty = 0.0f;
    bool enPumpSupply = false;

    switch(pump)
    {
        case pumps_t::MAIN_PUMP:
        {
            if(dir == pump_dir::STOP)
            {
                gpioMainPump.set(false);
                duty = 0.0f;
            }
            else
            {
                gpioMainPump.set(true);
                duty = 1.0f;
            }   
            break;
        }
        case pumps_t::PH_PLUS:
        {
            if(dir == pump_dir::STOP)
            {
                pwmPHplusRev.set(false);
                pwmPHplusDose.set(false);
                duty = 0.0f;
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmPHplusRev.set(false);
                pwmPHplusDose.set(true);
                duty = pwmPHplusDose.getDuty();
                enPumpSupply = true;
            }
            else
            {
                pwmPHplusDose.set(false);
                pwmPHplusRev.set(true);
                duty = pwmPHplusRev.getDuty();
                enPumpSupply = true;
            }
            break;
        }
        case pumps_t::PH_MINUS:
        {
            if(dir == pump_dir::STOP)
            {
                pwmPHminusRev.set(false);
                pwmPHminusDose.set(false);    
                duty = 0.0f;        
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmPHminusRev.set(false);
                pwmPHminusDose.set(true);
                duty = pwmPHminusDose.getDuty();
                enPumpSupply = true;
            }
            else
            {
                pwmPHminusDose.set(false);
                pwmPHminusRev.set(true);
                duty = pwmPHminusRev.getDuty();
                enPumpSupply = true;
            }
            break;
        }
        case pumps_t::FERTILIZER_A:
        {
            if(dir == pump_dir::STOP)
            {
                pwmFertilizerARev.set(false);
                pwmFertilizerADose.set(false);
                duty = 0.0f;
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmFertilizerARev.set(false);
                pwmFertilizerADose.set(true);
                duty = pwmFertilizerADose.getDuty();
                enPumpSupply = true;
            }
            else
            {
                pwmFertilizerADose.set(false);
                pwmFertilizerARev.set(true);
                duty = pwmFertilizerARev.getDuty();
                enPumpSupply = true;
            }
            break;
        }
        case pumps_t::FERTILIZER_B:
        {
            if(dir == pump_dir::STOP)
            {
                pwmFertilizerBRev.set(false);
                pwmFertilizerBDose.set(false);
                duty = 0.0f;
            }
            else if(dir == pump_dir::PUMP)
            {
                pwmFertilizerBRev.set(false);
                pwmFertilizerBDose.set(true);
                duty = pwmFertilizerBDose.getDuty();
                enPumpSupply = true;
            }
            else
            {
                pwmFertilizerBDose.set(false);
                pwmFertilizerBRev.set(true);
                duty = pwmFertilizerBRev.getDuty();
                enPumpSupply = true;
            }
            break;
        }
    }

    // disable later in IDLE
    if(enPumpSupply)
    {
        gpioEnDevices.set(true);
    }

    web_pumps(pump, dir, duty);
}