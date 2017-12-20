#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include "config.h"

class Aktualizr : public boost::noncopyable {
 public:
  Aktualizr(const Config& config);

  int run();

 private:
  const Config& config_;
};

#endif  // AKTUALIZR_H_
