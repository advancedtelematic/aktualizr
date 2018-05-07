#ifndef LT_EXECUTOR_H
#define LT_EXECUTOR_H

#include <atomic>
#include <boost/thread/latch.hpp>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>
#include "logging/logging.h"
#include "stats.h"

namespace timer = std::chrono;

class ExecutionController {
 public:
  virtual void stop() = 0;

  virtual bool claim() = 0;
};

class UnboundedExecutionController : ExecutionController {
 private:
  std::atomic_bool stopped;

 public:
  UnboundedExecutionController() : stopped{false} {}

  void stop() override { stopped = true; }

  bool claim() override { return stopped; }
};

class FixedExecutionController : public ExecutionController {
 private:
  std::atomic_uint iterations;

 public:
  FixedExecutionController(const uint i) : iterations{i} {}

  void stop() override { iterations.store(0); }

  bool claim() override {
    while (true) {
      auto i = iterations.load();
      if (i == 0) {
        return false;
      } else if (iterations.compare_exchange_strong(i, i - 1)) {
        return true;
      }
    }
  }
};

class ThreadJoiner {
  std::vector<std::thread> &threads;

 public:
  explicit ThreadJoiner(std::vector<std::thread> &threads_) : threads(threads_) {}
  ~ThreadJoiner() {
    for (size_t i = 0; i < threads.size(); i++) {
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }
};

typedef timer::steady_clock::time_point TimePoint;
class TaskStartTimeCalculator {
  TimePoint startTime;
  const timer::duration<int, std::milli> taskInterval;
  std::atomic_ulong taskIndex;

 public:
  TaskStartTimeCalculator(const unsigned rate) : startTime{}, taskInterval{std::milli::den / rate}, taskIndex{0} {}

  void start() { startTime = timer::steady_clock::now(); }

  TimePoint operator()() {
    auto i = ++taskIndex;
    return startTime + taskInterval * i;
  }
};

template <typename TaskStream>
class Executor {
  ExecutionController &controller;
  std::vector<std::thread> workers;
  std::vector<Statistics> statistics;
  TaskStartTimeCalculator calculateTaskStartTime;
  boost::latch threadCountDown;
  boost::latch starter;

  void runWorker(TaskStream &tasks, Statistics &stats) {
    using clock = std::chrono::steady_clock;
    LOG_DEBUG << "Worker created: " << std::this_thread::get_id();
    threadCountDown.count_down();
    starter.wait();
    while (controller.claim()) {
      auto task = tasks.nextTask();
      const auto intendedStartTime = calculateTaskStartTime();
      if (timer::steady_clock::now() < intendedStartTime) {
        std::this_thread::sleep_until(intendedStartTime);
      }
      const clock::time_point start = clock::now();
      task();
      const clock::time_point end = clock::now();
      std::chrono::milliseconds executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      stats.recordSuccess(executionTime);
    }
    LOG_DEBUG << "Worker finished execution: " << std::this_thread::get_id();
  }

 public:
  Executor(std::vector<TaskStream> &feeds, const unsigned rate, ExecutionController &ctrl)
      : controller{ctrl},
        workers{},
        statistics(feeds.size()),
        calculateTaskStartTime{rate},
        threadCountDown{feeds.size()},
        starter{1} {
    workers.reserve(feeds.size());
    try {
      for (size_t i = 0; i < feeds.size(); i++) {
        workers.push_back(std::thread(&Executor::runWorker, this, std::ref(feeds[i]), std::ref(statistics[i])));
      }
    } catch (...) {
      controller.stop();
      throw;
    }
  };

  Statistics run() {
    Statistics summary{};
    // wait till all threads are crerated and ready to go
    threadCountDown.wait();
    calculateTaskStartTime.start();
    summary.start();
    // start execution
    starter.count_down();
    // wait till all threads finished execution
    for (size_t i = 0; i < workers.size(); i++) {
      if (workers[i].joinable()) {
        workers[i].join();
      }
    }

    summary.stop();
    for (size_t i = 0; i < statistics.size(); i++) {
      summary += statistics[i];
    }
    summary.print();
    return summary;
  };
};

#endif