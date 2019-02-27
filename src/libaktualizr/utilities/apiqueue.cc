#include "apiqueue.h"
#include "logging/logging.h"

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

CommandQueue::~CommandQueue() {
  try {
    {
      std::lock_guard<std::mutex> l(m_);
      shutdown_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  } catch (std::exception& ex) {
    LOG_ERROR << "~CommandQueue() exception: " << ex.what() << std::endl;
  } catch (...) {
    LOG_ERROR << "~CommandQueue() unknown exception" << std::endl;
  }
}

void CommandQueue::run() {
  thread_ = std::thread([this] {
    std::unique_lock<std::mutex> lock(m_);
    for (;;) {
      cv_.wait(lock, [this] { return (!queue_.empty() && !paused_) || shutdown_; });
      if (shutdown_) {
        break;
      }
      auto task = std::move(queue_.front());
      queue_.pop();
      lock.unlock();
      task();
      lock.lock();
    }
  });
}

bool CommandQueue::pause(bool do_pause) {
  bool has_effect;
  {
    std::lock_guard<std::mutex> lock(m_);
    has_effect = paused_ != do_pause;
    paused_ = do_pause;
    token_.setPause(do_pause);
  }
  cv_.notify_all();

  return has_effect;
}

void CommandQueue::abort() {}
}  // namespace api
