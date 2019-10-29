#include "sig_handler.h"
#include "logging/logging.h"

std::atomic_uint SigHandler::signal_marker_;
std::mutex SigHandler::exit_m_;
std::condition_variable SigHandler::exit_cv_;
bool SigHandler::exit_flag_;

SigHandler& SigHandler::get() {
  static SigHandler handler;
  return handler;
}

SigHandler::~SigHandler() {
  try {
    {
      std::lock_guard<std::mutex> g(exit_m_);
      exit_flag_ = true;
    }
    exit_cv_.notify_all();

    if (polling_thread_.joinable()) {
      polling_thread_.join();
    }
  } catch (...) {
  }
}

void SigHandler::start(const std::function<void()>& on_signal) {
  if (polling_thread_.get_id() != boost::thread::id()) {
    throw std::runtime_error("SigHandler can only be started once");
  }

  polling_thread_ = boost::thread([on_signal]() {
    std::unique_lock<std::mutex> l(exit_m_);
    while (true) {
      auto got_signal = signal_marker_.exchange(0);

      if (got_signal) {
        on_signal();
        return;
      }

      if (exit_cv_.wait_for(l, std::chrono::seconds(1), [] { return exit_flag_; })) {
        break;
      }
    }
  });
}

void SigHandler::signal(int sig) { ::signal(sig, signal_handler); }

void SigHandler::signal_handler(int sig) {
  (void)sig;
  unsigned int v = 0;
  // put true if currently set to false
  SigHandler::signal_marker_.compare_exchange_strong(v, 1);
}
