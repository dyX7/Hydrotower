#pragma once

#include <Arduino.h>
#include <functional>
#include <settings.hpp>


// ---------------- Time Units ----------------

struct Time {
  uint64_t us;

  static Time us_(uint64_t v);
  static Time ms(uint64_t v);
  static Time sec(uint64_t v);
  static Time min(uint64_t v);
  static Time hours(uint64_t v);
  static Time days(uint64_t v);
};

// ---------------- Scheduler ----------------

#define MAX_TASKS 10

using TaskCallback = std::function<void()>;

class scheduler_t {
public:
  // returns task ID (-1 if failed)
  bool addTask(int &id, TaskCallback cb, Time t);
  bool removeTask(int &id);

  bool enableTask(int id);
  bool disableTask(int id);
  bool updateInterval(int id, Time t);

  void poll();

private:
  struct Task {
    TaskCallback callback;
    uint64_t interval;
    uint64_t lastRun;
    bool active;
    int id;
  };

  Task tasks[MAX_TASKS];
  int taskCount = 0;
  int nextId = 1;
};