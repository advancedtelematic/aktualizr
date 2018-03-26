#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include "config.h"

class Aktualizr : public boost::noncopyable {
 public:
  Aktualizr(Config& config);

  int run();

 private:
  Config& config_;
};

#endif  // AKTUALIZR_H_
