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

void TestBusPrimary::sendPrivateKey(const std::string &ecu_serial, const std::string &key) {
  std::vector<Secondary>::iterator it;
  bool found = false;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    if (ecu_serial == it->getEcuSerial()){
      it->setPrivateKey(key);
      found = true;
    }
  }
  if(!found){
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  }
}

};
