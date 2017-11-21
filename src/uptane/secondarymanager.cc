#include "uptane/secondarymanager.h"

namespace Uptane {
SecondaryManager::SecondaryManager(std::vector<SecondaryInterface*>& secondaries) {
  std::vector<SecondaryInterface*>::iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); ++it) secondaries_[it->getEcuSerial()] = *it;
}

Json::Value SecondaryManager::getManifest(const std::string& ecu_serial) {
  if (secondaries_.find(ecu_serial) == secondaries.end())
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
    return secondaries_[ecu_serial].genAndSendManifest();
}

Json::Value SecondaryManager::sendMetaPartial(const Root& root_meta,
                                              const Targets& targets_meta) {
  if (secondaries_.find(ecu_serial) == secondaries.end())
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
    return secondaries_[ecu_serial].verifyMeta(root_meta, targets_meta);
}

Json::Value SecondaryManager::sendMetaFull(const std::string& ecu_serial,
                                           const struct MetaPack* meta_pack) {
  throw std::runtime_error("SecondaryManager only supports partial verification");
}

Json::Value SecondaryManager::sendFirmware(const uint8_t* blob, size_t size) {
  if (secondaries_.find(ecu_serial) == secondaries.end())
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
    return secondaries_[ecu_serial].writeImage(blob, size);
}

void SecondaryManager::getPublicKey(const std::string& ecu_serial, std::string* keytype, std::string* key) {
  if (secondaries_.find(ecu_serial) == secondaries.end())
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
    secondaries_[ecu_serial].getPublicKey(keytype, _key);
}

void SecondaryManager::setKeys(const std::string& ecu_serial, const std::string& public_key,
                               const std::string& private_key) {
  if (secondaries_.find(ecu_serial) == secondaries.end())
    throw std::runtime_error("ecu_serial - " + ecu_serial + " not found");
  else
    return secondaries_[ecu_serial].setKeys(public_key, private_key);
}
}
