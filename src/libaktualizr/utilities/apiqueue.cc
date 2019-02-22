#include "apiqueue.h"

namespace api {

bool FlowControlToken::setPause(bool set_paused) {
  {
    std::lock_guard<std::mutex> lock(m_);
    if (set_paused && state_ == State::kRunning) {
      state_ = State::kPaused;
    } else if (!set_paused && state_ == State::kPaused) {
      state_ = State::kRunning;
    } else {
      return false;
    }
  }
  cv_.notify_all();
  return true;
}

bool FlowControlToken::setAbort() {
  {
    std::lock_guard<std::mutex> g(m_);
    if (state_ == State::kAborted) {
      return false;
    }
    state_ = State::kAborted;
  }
  cv_.notify_all();
  return true;
}

bool FlowControlToken::canContinue(bool blocking) const {
  std::unique_lock<std::mutex> lk(m_);
  if (blocking) {
    cv_.wait(lk, [this] { return state_ != State::kPaused; });
  }
  return state_ == State::kRunning;
}

}  // namespace Api
