#ifndef LT_EXECUTOR_H
#define LT_EXECUTOR_H

#ifdef BUILD_OSTREE
#include <glib.h>
#endif
#include <atomic>
#include <boost/thread/latch.hpp>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include "logging/logging.h"
#include "stats.h"

namespace timer = std::chrono;

class ExecutionController {
 public:
  virtual ~ExecutionController() = default;
  virtual void stop() = 0;

  virtual bool claim() = 0;
};

class UnboundedExecutionController : ExecutionController {
 private:
  std::atomic_bool stopped;

 public:
  UnboundedExecutionController() : stopped{false} {}

  void stop() override { stopped = true; }

  bool claim() override { return !stopped; }
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

class InterruptableExecutionController : public ExecutionController {
 private:
  std::atomic_bool stopped;
  static std::atomic_bool interrupted;
  static void handleSignal(int) {
    LOG_INFO << "SIGINT received";
    interrupted = true;
  }

 public:
  InterruptableExecutionController() : stopped{false} {
    std::signal(SIGINT, InterruptableExecutionController::handleSignal);
  };

  bool claim() override { return !(interrupted || stopped); }

  void stop() override { stopped = true; }
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
  std::unique_ptr<ExecutionController> controller;
  std::vector<std::thread> workers;
  std::vector<Statistics> statistics;
  TaskStartTimeCalculator calculateTaskStartTime;
  boost::latch threadCountDown;
  boost::latch starter;
  const std::string label;

  void runWorker(TaskStream &tasks, Statistics &stats) {
#ifdef BUILD_OSTREE
    GMainContext *thread_context = g_main_context_new();
    g_main_context_push_thread_default(thread_context);
#endif
    using clock = std::chrono::steady_clock;
    LOG_DEBUG << label << ": Worker created: " << std::this_thread::get_id();
    threadCountDown.count_down();
    starter.wait();
    while (controller->claim()) {
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
    LOG_DEBUG << label << ": Worker finished execution: " << std::this_thread::get_id();
#ifdef BUILD_OSTREE
    g_main_context_pop_thread_default(thread_context);
    g_main_context_unref(thread_context);
#endif
  }

 public:
  Executor(std::vector<TaskStream> &feeds, const unsigned rate, std::unique_ptr<ExecutionController> ctrl,
           const std::string lbl)
      : controller{std::move(ctrl)},
        workers{},
        statistics(feeds.size()),
        calculateTaskStartTime{rate},
        threadCountDown{feeds.size()},
        starter{1},
        label{lbl} {
    workers.reserve(feeds.size());
    try {
      for (size_t i = 0; i < feeds.size(); i++) {
        workers.push_back(std::thread(&Executor::runWorker, this, std::ref(feeds[i]), std::ref(statistics[i])));
      }
    } catch (...) {
      controller->stop();
      throw;
    }
  };

  Statistics run() {
    Statistics summary{};
    // wait till all threads are crerated and ready to go
    LOG_INFO << label << ": Waiting for threads to start";
    threadCountDown.wait();
    calculateTaskStartTime.start();
    summary.start();
    LOG_INFO << label << ": Starting tests";
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
    std::cout << "Results for: " << label << std::endl;
    summary.print();
    return summary;
  };
};

#endif
