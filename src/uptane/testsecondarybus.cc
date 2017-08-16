#include "testsecondarybus.h"

namespace Uptane {
TestSecondaryBus::TestSecondaryBus(std::vector<VirtualSecondary> &secondaries) {
  std::vector<VirtualSecondary>::iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); ++it)
    secondaries_[it->getEcuSerial()] = *it;
}

Json::Value TestSecondaryBus::getManifest(const std::string& ecu_serial) {
  if(secondaries_.find(ecu_serial) == secondaries.end())
	  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
	  return secondaries_[ecu_serial].genAndSendManifest();
}

Json::Value TestSecondaryBus::sendMetaPartial(const TimeMeta& time_meta, const Root& root_meta, const Targets& targets_meta) {
  if(secondaries_.find(ecu_serial) == secondaries.end())
	  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
	  return secondaries_[ecu_serial].verifyMeta(time_meta, root_meta, targets_meta);
}

Json::Value TestSecondaryBus::sendMetaFull(const std::string& ecu_serial, const TimeMeta& time_meta, const struct MetaPack* meta_pack) {
  throw std::runtime_error("TestSecondaryBus only supports partial verification");
}

Json::Value TestSecondaryBus::sendFirmware(const uint8_t* blob, size_t size) { 
  if(secondaries_.find(ecu_serial) == secondaries.end())
	  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
	  return secondaries_[ecu_serial].writeImage(blob, size);
}

void TestSecondaryBus::getPublicKey(const std::string &ecu_serial, std::string *keytype, std::string *key) {
  if(secondaries_.find(ecu_serial) == secondaries.end())
	  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
	  secondaries_[ecu_serial].getPublicKey(keytype, _key);
}

void TestSecondaryBus::setKeys(const std::string &ecu_serial, const std::string &public_key,
                              const std::string &private_key) {
  if(secondaries_.find(ecu_serial) == secondaries.end())
	  throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
	  return secondaries_[ecu_serial].setKeys(public_key, private_key);
}

}
