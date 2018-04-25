#ifndef UPTANE_OPCUASECONDARY_H_
#define UPTANE_OPCUASECONDARY_H_

#include "json/json.h"
#include "secondaryinterface.h"

#include "utilities/types.h"

namespace Uptane {

struct MetaPack;
class SecondaryConfig;

class OpcuaSecondary : public SecondaryInterface {
 public:
  explicit OpcuaSecondary(const SecondaryConfig& /*sconfig*/);
  ~OpcuaSecondary() override;

  std::string getSerial() override;
  std::string getHwId() override;
  std::pair<KeyType, std::string> getPublicKey() override;

  Json::Value getManifest() override;
  bool putMetadata(const MetaPack& meta_pack) override;

  bool sendFirmware(const std::string& data) override;

  int getRootVersion(bool director) override;
  bool putRoot(Uptane::Root root, bool director) override;
};

}  // namespace Uptane

#endif  // UPTANE_OPCUASECONDARY_H_
