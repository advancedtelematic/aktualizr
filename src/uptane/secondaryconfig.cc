#include "secondaryconfig.h"

namespace Uptane {

std::string SecondaryConfig::ecu_private_key() const {
  boost::filesystem::path ecu_private_key(ecu_private_key_);
  if (ecu_private_key.is_absolute() || full_client_dir.empty()) {
    return ecu_private_key_;
  } else {
    return (full_client_dir / ecu_private_key).string();
  }
}

std::string SecondaryConfig::ecu_public_key() const {
  boost::filesystem::path ecu_public_key(ecu_public_key_);
  if (ecu_public_key.is_absolute() || full_client_dir.empty()) {
    return ecu_public_key_;
  } else {
    return (full_client_dir / ecu_public_key).string();
  }
}
}
