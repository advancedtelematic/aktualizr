#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <queue>

/**
 * A thread-safe channel, similar to Go.
 */
template <typename T> class Channel {
public:
  /**
   * Initialise a new channel that can have at most `max_in_flight` unread entries.
   */
  Channel()
      : max_in_flight(100), closed(false) {}
  /**
   * Indicate there will never be more data in the channel. Future reads will fail.
   */
  void close() {
    boost::unique_lock<boost::mutex> lock(m);
    closed = true;
    drain_cv.notify_one();
  }
  
  bool hasSpace(){
    return q.size() < max_in_flight;
  }
  
  bool hasValues(){
    return !q.empty() || closed; 
  }
  /**
   * Write a value to an open channel.
   *
   * If the channel is full, the write will block. If it is closed, it will throw.
   */
  void operator<<(const T &target) {
    boost::unique_lock<boost::mutex> lock(m);
    if (closed)
      throw std::logic_error("Attempt to write to closed channel.");
    fill_cv.wait(lock, boost::bind(&Channel::hasSpace, this));
    q.push(target);
    drain_cv.notify_one();
  }
  /**
   * Read a value.
   *
   * If no value is available, the read will block. This returns true if the read was successful, or false is the channel is closed. This makes it easy to use as the condition in a `while` loop.
   */
  bool operator>>(T &target) {
    boost::unique_lock<boost::mutex> lock(m);
    drain_cv.wait(lock, boost::bind(&Channel::hasValues, this));
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

private:
  size_t max_in_flight;
  bool closed;
  std::queue<T> q;
  boost::mutex m;
  boost::condition_variable fill_cv;
  boost::condition_variable drain_cv;
};

#endif