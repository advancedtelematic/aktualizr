#ifndef UPTANE_TESTBUS_SECONDARY_H_
#define UPTANE_TESTBUS_SECONDARY_H_

#include <string>
#include <vector>
#include "uptane/tufrepository.h"

namespace Uptane {

class Repository;

class TestBusSecondary {
 public:
  TestBusSecondary(Uptane::Repository *primary);
  std::string getImage(const Target &target);

 private:
  Uptane::Repository *primary_;
};
};

#endif