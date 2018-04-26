#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include "config/config.h"

class Aktualizr {
 public:
  explicit Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  int run();

 private:
  Config& config_;
};

#endif  // AKTUALIZR_H_
