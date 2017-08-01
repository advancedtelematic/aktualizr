#ifndef TIMER_H_
#define TIMER_H_

#include <boost/chrono.hpp>
#include <iostream>

/**
 * Elapsed time measurement
 */
class Timer : public boost::noncopyable {
 public:
  Timer();
  bool RunningMoreThan(double seconds) const;
  friend std::ostream& operator<<(std::ostream& os, const Timer&);

 private:
  typedef boost::chrono::steady_clock Clock;

  Clock::time_point start_;
};

#endif  // TIMER_H_
