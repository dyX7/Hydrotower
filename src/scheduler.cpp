#include "scheduler.hpp"
#include "web.hpp"

// ---------------- Time Implementation ----------------

Time Time::us_(uint64_t v)   { return Time{v}; }
Time Time::ms(uint64_t v)    { return Time{v * 1000ULL}; }
Time Time::sec(uint64_t v)   { return Time{v * 1000000ULL}; }
Time Time::min(uint64_t v)   { return Time{v * 60ULL * 1000000ULL}; }
Time Time::hours(uint64_t v) { return Time{v * 60ULL * 60ULL * 1000000ULL}; }
Time Time::days(uint64_t v)  { return Time{v * 24ULL * 60ULL * 60ULL * 1000000ULL}; }

// ---------------- Helper: readable time log ----------------
static String formatTime(uint64_t us)
{
  if (us >= 86400000000ULL) return String(us / 86400000000ULL) + "d";
  if (us >= 3600000000ULL)  return String(us / 3600000000ULL) + "h";
  if (us >= 60000000ULL)    return String(us / 60000000ULL) + "min";
  if (us >= 1000000ULL)     return String(us / 1000000ULL) + "s";
  if (us >= 1000ULL)        return String(us / 1000ULL) + "ms";
  return String(us) + "us";
}

uint32_t scheduler_t::getRemainingTime(task_t &task)
{
  if (task.index < 0 || task.index >= taskCount) return 0;

  auto &t = tasks[task.index];

  if (!t.active) return 0;

  uint64_t now = micros();
  uint64_t elapsed = now - t.lastRun;

  if (elapsed >= t.interval) return 0;

  return (t.interval - elapsed) / 1000000ULL; // seconds
}

bool scheduler_t::nextSecond()
{
  uint64_t now = micros();

  if (now - lastSecondTick >= 1000000ULL)
  {
    lastSecondTick += 1000000ULL;
    return true;
  }

  return false;
}

// ---------------- Scheduler ----------------

bool scheduler_t::addTask(task_t &task, TaskCallback cb, Time t, TaskCallback stopCb)
{
  if (taskCount >= MAX_TASKS) return false;

  // assign ID early
  task.id = nextId++;
  task.index = taskCount;

  // store task safely
  tasks[taskCount] = {
    cb,
    stopCb,
    t.us,
    micros(),
    false,
    task.id,
    task.name
  };

  taskCount++;

  uint64_t interval_us = t.us;

  web_log(String("SCHED + ") +
          task.name +
          "[id: " + task.id +
          "]\t" + formatTime(interval_us));
  return true;
}

bool scheduler_t::removeTask(task_t &task)
{
  if (task.id == -1) return false;

  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      // web_log(String("SCHED - ") + task.name + " id:" + task.id);

      for (int j = i; j < taskCount - 1; j++) {
        tasks[j] = tasks[j + 1];
      }

      taskCount--;
      task.id = -1;
      return true;
    }
  }

  task.id = -1;
  return false;
}

bool scheduler_t::startTask(task_t &task)
{
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      if (!tasks[i].active) {
        tasks[i].active = true;
        tasks[i].lastRun = micros();

        if (tasks[i].interval >= 1000000ULL) {
          web_log(String("SCHED ▶ ") + task.name + " " + formatTime(tasks[i].interval));
        } else {
          // web_log(String("SCHED ▶ ") + task.name);
        }
      }

      return true;
    }
  }
  return false;
}

bool scheduler_t::stopTask(task_t &task)
{
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      if (tasks[i].active) {
        tasks[i].active = false;

        if (tasks[i].stopCallback) {
          tasks[i].stopCallback();
        }

        // web_log(String("SCHED ⏸ ") + task.name);
      }

      return true;
    }
  }

  return false;
}

bool scheduler_t::setInterval(task_t &task, Time t)
{
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      tasks[i].interval = t.us;
      tasks[i].lastRun = micros();

      // web_log(String("SCHED ⟳ ") + task.name + " set " + formatTime(t.us));

      return true;
    }
  }

  return false;
}

bool scheduler_t::setHandler(task_t &task, TaskCallback cb)
{
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      tasks[i].callback = cb;

      web_log(String("SCHED ⚙ ") + task.name + " handler updated");

      return true;
    }
  }

  return false;
}

bool scheduler_t::setStopHandler(task_t &task, TaskCallback cb)
{
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == task.id) {

      tasks[i].stopCallback = cb;

      web_log(String("SCHED ⚙ ") + task.name + " stop handler updated");

      return true;
    }
  }

  return false;
}

// ---------------- Poll ----------------

void scheduler_t::poll()
{
  uint64_t now = micros();

  for (int i = 0; i < taskCount; i++) {
    auto &task = tasks[i];

    if (!task.active) continue;

    if (now - task.lastRun >= task.interval) {
      task.lastRun = now;
      task.callback();
    }
  }
}