#include "sig_handler.h"
#include "logging/logging.h"

void signal_handler(int sig) {
  (void)sig;
  bool v = false;
  // put true if currently set to false
  SigHandler::signal_marker_.compare_exchange_strong(v, true);
}

std::atomic<bool> SigHandler::signal_marker_;

SigHandler& SigHandler::get() {
  static SigHandler handler;
  return handler;
}

SigHandler::~SigHandler() {
  if (polling_thread_.joinable()) {
    polling_thread_.join();
  }
}

void SigHandler::start(const std::function<void()>& on_signal) {
  boost::unique_lock<boost::mutex> start_lock(m);

  if (polling_thread_.get_id() != boost::thread::id()) {
    throw std::runtime_error("SigHandler can only be started once");
  }

  polling_thread_ = boost::thread([this, on_signal]() {
    while (true) {
      bool signal = signal_marker_.exchange(false);

      if (signal) {
        LOG_INFO << "received KILL request";
        if (masked_secs_ != 0) {
          LOG_INFO << "KILL currently masked for " << masked_secs_ << " seconds, waiting...";
        }
        signal_pending = true;
      }

      if (signal_pending && masked_secs_ == 0) {
        LOG_INFO << "calling signal handler";
        on_signal();
        return;
      }

      boost::this_thread::sleep_for(boost::chrono::seconds(1));

      {
        boost::unique_lock<boost::mutex> lock(m);
        if (masked_secs_ > 0) {
          masked_secs_ -= 1;
        }
      }
    }
  });

  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
}

bool SigHandler::masked() {
  boost::unique_lock<boost::mutex> lock(m);

  return masked_secs_ != 0;
}

void SigHandler::mask(int secs) {
  boost::unique_lock<boost::mutex> lock(m);

  LOG_INFO << "masking signal for " << secs << " seconds";

  masked_secs_ = secs;
}
