#ifndef AKTUALIZR_APIQUEUE_H
#define AKTUALIZR_APIQUEUE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace api {

///
/// Provides a thread-safe way to pause and terminate task execution.
/// A task must call canContinue() method to check the current state.
///
class FlowControlToken {
 public:
  ///
  /// Called by the controlling thread to request the task to pause or resume.
  /// Has no effect if the task was aborted.
  /// @return `true` if the state was changed, `false` otherwise.
  ///
  bool setPause(bool set_paused);

  ///
  /// Called by the controlling thread to request the task to abort.
  /// @return `false` if the task was already aborted, `true` otherwise.
  ///
  bool setAbort();

  ///
  /// Called by the controlled thread to query the currently requested state.
  /// Sleeps if the state is `Paused` and `blocking == true`.
  /// @return `true` for `Running` state, `false` for `Aborted`,
  /// and also `false` for the `Paused` state, if the call is non-blocking.
  ///
  bool canContinue(bool blocking = true) const;

  ////
  //// Sets token to the initial state
  ////
  void reset();

 private:
  enum class State {
    kRunning,  // transitions: ->Paused, ->Aborted
    kPaused,   // transitions: ->Running, ->Aborted
    kAborted   // transitions: none
  } state_{State::kRunning};
  mutable std::mutex m_;
  mutable std::condition_variable cv_;
};

class CommandQueue {
 public:
  ~CommandQueue();
  void run();
  bool pause(bool do_pause);  // returns true iff pause→resume or resume→pause
  void abort(bool restart_thread = true);

  template <class R>
  std::future<R> enqueue(const std::function<R()>& f) {
    std::packaged_task<R()> task(f);
    auto r = task.get_future();
    {
      std::lock_guard<std::mutex> lock(m_);
      queue_.push(std::packaged_task<void()>(std::move(task)));
    }
    cv_.notify_all();
    return r;
  }

  template <class R>
  std::future<R> enqueue(const std::function<R(const api::FlowControlToken*)>& f) {
    std::packaged_task<R()> task(std::bind(f, &token_));
    auto r = task.get_future();
    {
      std::lock_guard<std::mutex> lock(m_);
      queue_.push(std::packaged_task<void()>(std::move(task)));
    }
    cv_.notify_all();
    return r;
  }

 private:
  std::atomic_bool shutdown_{false};
  std::atomic_bool paused_{false};

  std::thread thread_;
  std::mutex thread_m_;

  std::queue<std::packaged_task<void()>> queue_;
  std::mutex m_;
  std::condition_variable cv_;
  FlowControlToken token_;
};

}  // namespace api
#endif  // AKTUALIZR_APIQUEUE_H
