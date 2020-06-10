#include "partialverificationsecondary.h"

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "logging/logging.h"
#include "primary/secondaryinterface.h"
#include "utilities/exceptions.h"
#include "utilities/types.h"

namespace Uptane {

PartialVerificationSecondary::PartialVerificationSecondary(Primary::PartialVerificationSecondaryConfig sconfig_in)
    : sconfig(std::move(sconfig_in)), root_(Root::Policy::kAcceptAll) {
  boost::filesystem::create_directories(sconfig.metadata_path);

  // TODO(OTA-2484): Probably we need to generate keys on the secondary
  std::string public_key_string;
  if (!loadKeys(&public_key_string, &private_key_)) {
    if (!Crypto::generateKeyPair(sconfig.key_type, &public_key_string, &private_key_)) {
      LOG_ERROR << "Could not generate keys for secondary " << PartialVerificationSecondary::getSerial() << "@"
                << sconfig.ecu_hardware_id;
      throw std::runtime_error("Unable to generate secondary keys");
    }
    storeKeys(public_key_string, private_key_);
  }
  public_key_ = PublicKey(public_key_string, sconfig.key_type);
}

bool PartialVerificationSecondary::putMetadata(const Target &target) {
  (void)target;
  TimeStamp now(TimeStamp::Now());
  detected_attack_.clear();

  std::string director_root;
  std::string director_targets;
  if (!secondary_provider_->getDirectorMetadata(&director_root, &director_targets)) {
    LOG_ERROR << "Unable to read Director metadata.";
    return false;
  }

  // TODO(OTA-2484): check for expiration and version downgrade
  root_ = Uptane::Root(RepositoryType::Director(), Utils::parseJSON(director_root), root_);
  Uptane::Targets targets(RepositoryType::Director(), Role::Targets(), Utils::parseJSON(director_targets),
                          std::make_shared<Uptane::Root>(root_));
  if (meta_targets_.version() > targets.version()) {
    detected_attack_ = "Rollback attack detected";
    return true;
  }
  meta_targets_ = targets;
  return true;
}

Uptane::Manifest PartialVerificationSecondary::getManifest() const {
  throw NotImplementedException();
  return Json::Value();
}

int PartialVerificationSecondary::getRootVersion(bool director) const {
  (void)director;
  throw NotImplementedException();
  return 0;
}

bool PartialVerificationSecondary::putRoot(const std::string &root, bool director) {
  (void)root;
  (void)director;

  throw NotImplementedException();
  return false;
}

data::ResultCode::Numeric PartialVerificationSecondary::sendFirmware(const Uptane::Target &target) {
  (void)target;
  throw NotImplementedException();
}

data::ResultCode::Numeric PartialVerificationSecondary::install(const Uptane::Target &target) {
  (void)target;
  throw NotImplementedException();
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
}  // namespace Uptane
