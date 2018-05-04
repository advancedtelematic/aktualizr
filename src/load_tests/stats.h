#ifndef LT_STATS_H
#define LT_STATS_H

#include <hdr_histogram.h>
#include <chrono>

class Histogram {
  hdr_histogram* histogram;

 public:
  Histogram();
  ~Histogram();
  void record(const std::chrono::milliseconds& duration) { hdr_record_value(histogram, duration.count()); };

  Histogram& operator+=(const Histogram& rhs);
  void print();
  int64_t totalCount();
};

class Statistics {
  using clock = std::chrono::steady_clock;
  clock::time_point startedAt;
  clock::time_point finishedAt;
  Histogram successDurations;
  Histogram errorDurations;

 public:
  Statistics() : successDurations{}, errorDurations{} {}

  void recordSuccess(const std::chrono::milliseconds& duration);
  void recordFailure(const std::chrono::milliseconds& durarion);
  void start();
  void stop();
  void print();
  Statistics& operator+=(const Statistics& rhs);
};

#endif