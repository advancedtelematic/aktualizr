#ifndef UPTANE_VIRTUALSECONDARY_H_
#define UPTANE_VIRTUALSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "types.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

namespace Uptane {
class VirtualSecondary : public SecondaryInterface {
 public:
  VirtualSecondary(const SecondaryConfig &sconfig_in) : SecondaryInterface(sconfig_in);

  //void setKeys(const std::string &public_key, const std::string &private_key);
  //bool writeImage(const uint8_t *blob, size_t size);
  //Json::Value genAndSendManifest(Json::Value custom = Json::Value(Json::nullValue));

  virtual std::string getPublicKey() { return public_key; }
  virtual Json::Value getManifest();
  virtual bool putMetadata(const MetaPack& meta_pack);
  virtual int getRootVersion(bool director);
  virtual bool putRoot(Uptane::Root root, bool director);

  virtual bool sendFirmware(const uint8_t* blob, size_t size) = 0;



 private:
  std::string public_key;
  std::string private_key;

  std::string detected_attack;
  std::string expected_target_name;
  std::vector<Hash> expected_target_hashes;
  int64_t expected_target_length;

  MetaPack current_meta;

  void VirtualSecondary::storeKeys(const std::string &public_key, const std::string &private_key);
  bool VirtualSecondary::loadKeys(std::string *public_key, std::string *private_key);

  void VirtualSecondary::storeMetadata(const MetaPack& meta_pack);
  bool VirtualSecondary::loadMetadata(MetaPack* meta_pack);
};
}

#endif  // UPTANE_VIRTUALSECONDARY_H_
