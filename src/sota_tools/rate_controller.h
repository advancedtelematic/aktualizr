#ifndef SOTA_CLIENT_TOOLS_RATE_CONTROLLER_H_
#define SOTA_CLIENT_TOOLS_RATE_CONTROLLER_H_

#include <chrono>

/**
 * Control the rate of outgoing requests.
 * This receives signals from the network layer when a request finishes of the form (start time, end time, success).
 * It generates controls for the network layer in the form of:
 *    MaxConcurrency - The current estimate of the number of parallel requests that can be opened
 *    Sleep() - The number of seconds to sleep before sending the next request. 0.0 if MaxConcurrency is > 1
 *    Failed() - A boolean indicating that the server is broken, and to report an error up to the user.
 * The congestion control is loosely based on the original TCP AIMD scheme. Better performance might be available by
 * Stealing ideas from the later TCP conjection control algorithms
 */
class RateController {
 public:
  using clock = std::chrono::steady_clock;
  explicit RateController(int concurrency_cap = 30);
  RateController(const RateController&) = delete;
  RateController operator=(const RateController&) = delete;

  void RequestCompleted(clock::time_point start_time, clock::time_point end_time, bool succeeded);

  int MaxConcurrency() const;

  clock::duration GetSleepTime() const;

  bool ServerHasFailed() const;

 private:
  /**
    * After sleeping this long and still getting a 500 error, assume the
    * server has failed permanently
    */
  static const clock::duration kMaxSleepTime;

  /**
   * After getting a failure with a concurrency of 1, sleep for this long
   * before retrying. Following retries grow exponentially to kMaxSleepTime.
   */
  static const clock::duration kInitialSleepTime;

  const int concurrency_cap_;
  /**
   * After making a change to the system, we wait a full round-trip time to
   * see any effects of the change. This is the last time that an change was
   * made, and only requests that started after this time will be considered
   * to be 'new' information.
   */
  clock::time_point last_concurrency_update_;
  int max_concurrency_{1};
  clock::duration sleep_time_{0};

  void CheckInvariants() const;
};

#endif  // SOTA_CLIENT_TOOLS_RATE_CONTROLLER_H_
