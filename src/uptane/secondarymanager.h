#ifndef UPTANE_SECONDARYMANAGER_H_
#define UPTANE_SECONDARYMANAGER_H_

#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

#include <vector>

/* Json snippet returned by sendMetaXXX():
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */
namespace Uptane {
class SecondaryManager {
 public:
  SecondaryManager(std::vector<SecondaryInterface*>& secondaries);
  virtual Json::Value getManifest(const std::string& ecu_serial);
  virtual Json::Value sendMetaPartial(const Root& root_meta, const Targets& targets_meta);
  virtual Json::Value sendMetaFull(const struct MetaPack& meta_pack);
  virtual bool sendFirmware(const uint8_t* blob, size_t size);
  virtual void getPublicKey(const std::string& ecu_serial, std::string* keytype, std::string* key);
  virtual void setKeys(const std::string& ecu_serial, const std::string& keytype, const std::string& public_key,
                       const std::string& private_key);

 private:
  std::map<std::string, std::vector<SecondaryInterface*> > secondaries_;
};
}

#endif  // UPTANE_SECONDARYMANAGER_H_
