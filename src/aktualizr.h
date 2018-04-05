#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include "config.h"

class Aktualizr {
 public:
  Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  int run();

 private:
  Config& config_;
};

#endif  // AKTUALIZR_H_
