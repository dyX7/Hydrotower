#include "scheduler.hpp"

// ---------------- Time Implementation ----------------

Time Time::us_(uint64_t v)   { return Time{v}; }
Time Time::ms(uint64_t v)    { return Time{v * 1000ULL}; }
Time Time::sec(uint64_t v)   { return Time{v * 1000000ULL}; }
Time Time::min(uint64_t v)   { return Time{v * 60ULL * 1000000ULL}; }
Time Time::hours(uint64_t v) { return Time{v * 60ULL * 60ULL * 1000000ULL}; }
Time Time::days(uint64_t v)  { return Time{v * 24ULL * 60ULL * 60ULL * 1000000ULL}; }

// ---------------- Scheduler ----------------

int scheduler_t::addTask(TaskCallback cb, Time t) {
  if (taskCount >= MAX_TASKS) return -1;

  int id = nextId++;

  tasks[taskCount++] = {
    cb,
    t.us,
    micros(),
    true,
    id
  };

  return id;
}

bool scheduler_t::removeTask(int id) {
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == id) {
      for (int j = i; j < taskCount - 1; j++) {
        tasks[j] = tasks[j + 1];
      }
      taskCount--;
      return true;
    }
  }
  return false;
}

bool scheduler_t::enableTask(int id) {
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == id) {
      tasks[i].active = true;
      tasks[i].lastRun = micros();
      return true;
    }
  }
  return false;
}

bool scheduler_t::disableTask(int id) {
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == id) {
      tasks[i].active = false;
      return true;
    }
  }
  return false;
}

bool scheduler_t::updateInterval(int id, Time t) {
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].id == id) {
      tasks[i].interval = t.us;
      return true;
    }
  }
  return false;
}

// ---------------- Poll ----------------

void scheduler_t::poll() {
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