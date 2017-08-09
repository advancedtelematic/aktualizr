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

Json::Value TestBusPrimary::sendTargets(const std::vector<Uptane::Target> &targets) {
  Json::Value manifests(Json::arrayValue);
  std::vector<Secondary>::iterator it;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    manifests.append((*it).newTargetsCallBack(targets));
  }
  return manifests;
}

void TestBusPrimary::sendKeys(const std::string &ecu_serial, const std::string &public_key,
                              const std::string &private_key) {
  std::vector<Secondary>::iterator it;
  bool found = false;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    if (ecu_serial == it->getEcuSerial()) {
      it->setKeys(public_key, private_key);
      found = true;
    }
  }
  if (!found) {
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  }
}

bool TestBusPrimary::reqPublicKey(const std::string &ecu_serial, std::string *keytype, std::string *key) {
  std::vector<Secondary>::iterator it;
  for (it = secondaries_->begin(); it != secondaries_->end(); ++it) {
    if (ecu_serial == it->getEcuSerial()) {
      *keytype = "RSA";
      return it->getPublicKey(key);
    }
  }
  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
}
}
