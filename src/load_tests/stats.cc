#include "stats.h"
#include <cstdlib>
#include <iostream>

Histogram::Histogram() { hdr_init(1, 3L * 60 * 1000, 3, &histogram); };

Histogram::~Histogram() { free(histogram); }
void Histogram::print() { hdr_percentiles_print(histogram, stdout, 5, 1.0, CLASSIC); }
Histogram &Histogram::operator+=(const Histogram &rhs) {
  hdr_add(histogram, rhs.histogram);
  return *this;
}
int64_t Histogram::totalCount() { return histogram->total_count; }

void Statistics::recordSuccess(const std::chrono::milliseconds &duration) { successDurations.record(duration); }

void Statistics::recordFailure(const std::chrono::milliseconds &duration) { errorDurations.record(duration); }

void Statistics::print() {
  successDurations.print();
  std::chrono::seconds elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(finishedAt - startedAt);
  std::cout << "Elapsed time: " << elapsedTime.count() << "s ";
  std::cout << "Rate: " << successDurations.totalCount() / elapsedTime.count() << "op/s";
}

Statistics &Statistics::operator+=(const Statistics &rhs) {
  successDurations += rhs.successDurations;
  return *this;
}

void Statistics::start() { startedAt = clock::now(); }
void Statistics::stop() { finishedAt = clock::now(); }
