#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include "config.h"

class Aktualizr {
 public:
  Aktualizr(const Config &config);
  int run();

 private:
  Aktualizr(Aktualizr const &);
  void operator=(Aktualizr const &);
  Config config_;
};

#endif  // AKTUALIZR_H_