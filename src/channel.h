#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

/**
 * A thread-safe channel, similar to Go.
 */
template <typename T>
class Channel {
 public:
  /**
   * Initialise a new channel that can have at most `max_in_flight` unread entries.
   */
  Channel() : timeout(std::chrono::milliseconds::max()) {}
  /**
   * Indicate there will never be more data in the channel. Future reads will fail.
   */
  void close() {
    std::unique_lock<std::mutex> lock(m);
    closed = true;
    drain_cv.notify_one();
  }

  bool hasSpace() { return q.size() < max_in_flight; }

  bool hasValues() { return !q.empty() || closed; }
  bool isClosed() { return closed; }
  /**
   * Write a value to an open channel.
   *
   * If the channel is full, the write will block. If it is closed, it will throw.
   */
  void operator<<(const T &target) {
    std::unique_lock<std::mutex> lock(m);
    if (closed) {
      return;
    }
    fill_cv.wait(lock, [this]() { return hasSpace(); });
    q.push(target);
    drain_cv.notify_one();
  }
  /**
   * Read a value.
   *
   * If no value is available, the read will block. This returns true if the read was successful, or false is the
   * channel is closed. This makes it easy to use as the condition in a `while` loop.
   */
  bool operator>>(T &target) {
    std::unique_lock<std::mutex> lock(m);
    if (timeout == std::chrono::milliseconds::max()) {
      drain_cv.wait(lock, [this]() { return hasValues(); });
    } else {
      drain_cv.wait_for(lock, timeout, [this]() { return hasValues(); });
    }

    if (q.empty()) {
      return false;
    }

    if (q.size() >= max_in_flight) {
      fill_cv.notify_one();
    }
    if (!q.empty()) {
      target = q.front();
      q.pop();
      return true;
    }
    return false;
  }

  void clear() {
    std::unique_lock<std::mutex> lock(m);
    std::queue<T> empty;
    std::swap(q, empty);
  }

  void setTimeout(std::chrono::milliseconds to) { timeout = to; }

 private:
  size_t max_in_flight{100};
  bool closed{false};
  std::queue<T> q;
  std::mutex m;
  std::condition_variable fill_cv;
  std::condition_variable drain_cv;
  std::chrono::milliseconds timeout;
};

#endif
