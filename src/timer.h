#ifndef TIMER_H_
#define TIMER_H_

#include <chrono>
#include <iostream>

/**
 * Elapsed time measurement
 */
class Timer {
 public:
  Timer();
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  bool RunningMoreThan(double seconds) const;
  friend std::ostream& operator<<(std::ostream& os, const Timer&);

 private:
  typedef std::chrono::steady_clock Clock;

  Clock::time_point start_;
};

#endif  // TIMER_H_
