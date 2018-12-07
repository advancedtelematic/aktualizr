#include "rate_controller.h"

#include <algorithm>  // min
#include <cassert>

#include "logging/logging.h"

const RateController::clock::duration RateController::kMaxSleepTime = std::chrono::seconds(30);

const RateController::clock::duration RateController::kInitialSleepTime = std::chrono::seconds(1);

RateController::RateController(const int concurrency_cap) : concurrency_cap_(concurrency_cap) { CheckInvariants(); }

void RateController::RequestCompleted(const clock::time_point start_time, const clock::time_point end_time,
                                      const bool succeeded) {
  if (last_concurrency_update_ < start_time) {
    const int prev_concurrency = max_concurrency_;
    last_concurrency_update_ = end_time;
    if (succeeded) {
      max_concurrency_ = std::min(max_concurrency_ + 1, concurrency_cap_);
      sleep_time_ = clock::duration(0);
    } else {
      if (max_concurrency_ >= 2) {
        max_concurrency_ = max_concurrency_ / 2;
      } else {
        sleep_time_ = std::max(sleep_time_ * 2, kInitialSleepTime);
      }
    }
    if (prev_concurrency != max_concurrency_) {
      LOG_DEBUG << "Concurrency limit is now: " << max_concurrency_;
    }
  }
  CheckInvariants();
}

int RateController::MaxConcurrency() const {
  CheckInvariants();
  return max_concurrency_;
}

RateController::clock::duration RateController::GetSleepTime() const {
  CheckInvariants();
  return sleep_time_;
}

bool RateController::ServerHasFailed() const {
  CheckInvariants();
  return sleep_time_ > kMaxSleepTime;
}

void RateController::CheckInvariants() const {
  assert((sleep_time_ == clock::duration(0)) || (max_concurrency_ == 1));
  assert(0 < max_concurrency_);
  assert(max_concurrency_ <= concurrency_cap_);
}
