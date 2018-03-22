#ifndef UPTANE_OPCUASECONDARY_H_
#define UPTANE_OPCUASECONDARY_H_

#include "json/json.h"
#include "secondaryinterface.h"

#include <types.h>

namespace Uptane {

class MetaPack;
class SecondaryConfig;

class OpcuaSecondary : public SecondaryInterface {
 public:
  OpcuaSecondary(const SecondaryConfig&);
  virtual ~OpcuaSecondary();

  virtual std::string getSerial() override;
  virtual std::string getHwId() override;
  virtual std::pair<KeyType, std::string> getPublicKey() override;

  virtual Json::Value getManifest() override;
  virtual bool putMetadata(const MetaPack& meta_pack) override;

  virtual bool sendFirmware(const std::string& data) override;

  virtual int getRootVersion(bool director) override;
  virtual bool putRoot(Uptane::Root root, bool director) override;
};

}  // namespace Uptane

#endif  // UPTANE_OPCUASECONDARY_H_
