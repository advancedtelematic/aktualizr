#ifndef UPTANE_TESTPRIMARY_TRANSPORT_H_
#define UPTANE_TESTPRIMARY_TRANSPORT_H_

#include "uptane/secondary.h"
#include "uptane/tufrepository.h"

#include <vector>

/* Json snippet returned by sendMetaXXX(): 
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */
namespace Uptane {
class TestSecondaryBus:ISecondaryBus {
 public:
  TestSecondaryBus(std::vector<VirtualSecondary> &secondaries);
  virtual Json::Value getManifest(const std::string& ecu_serial);
  virtual Json::Value sendMetaPartial(const TimeMeta& time_meta, const Root& root_meta, const Targets& targets_meta);
  virtual Json::Value sendMetaFull(const TimeMeta& time_meta, const struct MetaPack& meta_pack);
  virtual bool sendFirmware(const uint8_t* blob, size_t size);
  virtual void getPublicKey(const std::string &ecu_serial, std::string *keytype, std::string * key);
  virtual void setKeys(const std::string &ecu_serial, const std::string& keytype, const std::string &public_key, const std::string &private_key);

 private:
  std::map<std::string, std::vector<VirtualSecondary>> secondaries_;
};
}

#endif
