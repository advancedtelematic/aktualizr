#include "timer.h"
#include <iomanip>

Timer::Timer() : start_(Timer::Clock::now()) {}

bool Timer::RunningMoreThan(double seconds) const {
  return start_ + std::chrono::duration<double>(seconds) < Timer::Clock::now();
}

std::ostream& operator<<(std::ostream& os, const Timer& timer) {
  Timer::Clock::duration elapsed = Timer::Clock::now() - timer.start_;
  std::chrono::duration<double> sec = elapsed;
  os << std::setprecision(3) << sec.count() << "s";
  return os;
}