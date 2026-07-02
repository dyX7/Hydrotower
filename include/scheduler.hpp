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

  uint64_t operator/(const Time& other) const 
  {
      return us / other.us;
  }
};

// ---------------- Task Handle ----------------

struct task_t {
  int id;
  int index;
  const char* name;

  task_t(const char* n = "", int i = -1) : id(i), name(n) {}
};

// ---------------- Scheduler ----------------

#define MAX_TASKS 20

using TaskCallback = std::function<void()>;

class scheduler_t {
public:
  uint32_t getRemainingTime(task_t &task);
  bool nextSecond();

  bool addTask(task_t &task, TaskCallback cb, Time t, TaskCallback stopCb = nullptr);
  bool removeTask(task_t &task);
  bool startTask(task_t &task);
  bool stopTask(task_t &task);
  bool setInterval(task_t &task, Time t);
  bool setHandler(task_t &task, TaskCallback cb);
  bool setStopHandler(task_t &task, TaskCallback cb);
  
  void poll();

private:
struct Task {
  TaskCallback callback;
  TaskCallback stopCallback;
  uint64_t interval;
  uint64_t lastRun;
  bool active;
  int id;
  const char* name;
};

  Task tasks[MAX_TASKS];
  int taskCount = 0;
  int nextId = 1;
  int64_t lastSecondTick = 0; 
};


