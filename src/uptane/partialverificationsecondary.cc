#include "uptane/partialverificationsecondary.h"

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "../exceptions.h"
#include "types.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

namespace Uptane {

PartialVerificationSecondary::PartialVerificationSecondary(const SecondaryConfig &sconfig_in)
    : SecondaryInterface(sconfig_in) {
  boost::filesystem::create_directories(sconfig.metadata_path);

  // FIXME Probably we need to generate keys on the secondary
  if (!loadKeys(&public_key_, &private_key_)) {
    if (!Crypto::generateKeyPair(sconfig.key_type, &public_key_, &private_key_)) {
      LOG_ERROR << "Could not generate keys for secondary " << PartialVerificationSecondary::getSerial() << "@"
                << sconfig.ecu_hardware_id;
      throw std::runtime_error("Unable to generate secondary keys");
    }
    storeKeys(public_key_, private_key_);
  }
  public_key_id_ = Crypto::getKeyId(public_key_);
}

bool PartialVerificationSecondary::putMetadata(const MetaPack &meta) {
  TimeStamp now(TimeStamp::Now());
  detected_attack_.clear();
  Uptane::Root root = meta.director_root;
  root.UnpackSignedObject(now, "director", Role::Targets(), meta.director_targets.original_object);
  if (meta_targets_.version > meta.director_targets.version) {
    detected_attack_ = "Rollback attack detected";
    return true;
  }
  meta_targets_ = meta.director_targets;
  std::vector<Uptane::Target>::const_iterator it;
  bool target_found = false;
  for (it = meta_targets_.targets.begin(); it != meta_targets_.targets.end(); ++it) {
    if (it->IsForSecondary(getSerial())) {
      if (target_found) {
        detected_attack_ = "Duplicate entry for this ECU";
        break;
      }
      target_found = true;
    }
  }
  return true;
}

Json::Value PartialVerificationSecondary::getManifest() {
  throw NotImplementedException();
  return Json::Value();
}

int PartialVerificationSecondary::getRootVersion(bool director) {
  (void)director;
  throw NotImplementedException();
  return 0;
}

bool PartialVerificationSecondary::putRoot(Uptane::Root root, bool director) {
  (void)root;
  (void)director;

  throw NotImplementedException();
  return false;
}

bool PartialVerificationSecondary::sendFirmware(const std::string &data) {
  (void)data;
  return false;
}

void PartialVerificationSecondary::storeKeys(const std::string &public_key, const std::string &private_key) {
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_private_key), private_key);
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_public_key), public_key);
}

bool PartialVerificationSecondary::loadKeys(std::string *public_key, std::string *private_key) {
  boost::filesystem::path public_key_path = sconfig.full_client_dir / sconfig.ecu_public_key;
  boost::filesystem::path private_key_path = sconfig.full_client_dir / sconfig.ecu_private_key;

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) {
    return false;
  }

  *private_key = Utils::readFile(private_key_path.string());
  *public_key = Utils::readFile(public_key_path.string());
  return true;
}
};