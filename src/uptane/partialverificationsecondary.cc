#include "uptane/partialverificationsecondary.h"

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "types.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

namespace Uptane {

PartialVerificationSecondary::PartialVerificationSecondary(const SecondaryConfig& sconfig_in)
    : SecondaryInterface(sconfig_in) {
  if (!Crypto::generateKeyPair(sconfig.key_type, &public_key, &private_key)) {
    LOG_ERROR << "Could not generate keys for secondary " << PartialVerificationSecondary::getSerial() << "@"
              << sconfig.ecu_hardware_id;
    throw std::runtime_error("Unable to generate secondary keys");
  }
}

bool PartialVerificationSecondary::putMetadata(const MetaPack& meta) {
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
};