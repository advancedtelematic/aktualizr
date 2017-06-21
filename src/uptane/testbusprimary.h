#ifndef UPTANE_TESTPRIMARY_TRANSPORT_H_
#define UPTANE_TESTPRIMARY_TRANSPORT_H_

#include "uptane/secondary.h"
#include "uptane/tufrepository.h"

#include <vector>

namespace Uptane {
class TestBusPrimary {
 public:
  TestBusPrimary(std::vector<Secondary> *secondaries);
  virtual Json::Value getManifests();
  virtual void sendTargets(const std::vector<Target> &targets);
  virtual void sendPrivateKey(const std::string &ecu_serial, const std::string &key);


 private:
  std::vector<Secondary> *secondaries_;
};
};

#endif