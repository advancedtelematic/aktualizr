#ifndef SIG_HANDLER_H
#define SIG_HANDLER_H

#include <atomic>
#include <csignal>
#include <functional>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class SigHandler {
 public:
  friend void signal_handler(int sig);
  static SigHandler& get();

  void start(const std::function<void()>& on_signal);

  bool masked();
  void mask(int secs);  // send 0 to unmask

 private:
  SigHandler() {}
  ~SigHandler();
  SigHandler(const SigHandler&) = delete;
  SigHandler& operator=(const SigHandler&) = delete;

  boost::thread polling_thread_;
  bool signal_pending;
  static std::atomic<bool> signal_marker_;

  boost::mutex m;
  int masked_secs_;
};

#endif /* SIG_HANDLER_H */
