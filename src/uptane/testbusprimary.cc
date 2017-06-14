#include "testbusprimary.h"

namespace Uptane {
TestBusPrimary::TestBusPrimary(std::vector<Secondary> *secondaries) : secondaries_(secondaries) {}

Json::Value TestBusPrimary::getManifests() {
  Json::Value manifests(Json::arrayValue);
  std::vector<Secondary>::iterator it;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    manifests.append((*it).genAndSendManifest());
  }
  return manifests;
}

void TestBusPrimary::sendTargets(const std::vector<Uptane::Target> &targets) {
  std::vector<Secondary>::iterator it;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    (*it).newTargetsCallBack(targets);
  }
}
};
