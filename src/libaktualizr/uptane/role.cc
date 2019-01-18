#include "uptane/tuf.h"

#include <sstream>

using Uptane::Role;
using Uptane::Version;

const std::string UPTANE_ROLE_ROOT = "root";
const std::string UPTANE_ROLE_SNAPSHOT = "snapshot";
const std::string UPTANE_ROLE_TARGETS = "targets";
const std::string UPTANE_ROLE_TIMESTAMP = "timestamp";

Role::Role(const std::string &role_name, const bool delegation) {
  std::string role_name_lower;
  std::transform(role_name.begin(), role_name.end(), std::back_inserter(role_name_lower), ::tolower);
  name_ = role_name_lower;
  if (delegation) {
    if (role_name_lower == UPTANE_ROLE_ROOT || role_name_lower == UPTANE_ROLE_SNAPSHOT ||
        role_name_lower == UPTANE_ROLE_TARGETS || role_name_lower == UPTANE_ROLE_TIMESTAMP) {
      throw Uptane::Exception("", "Invalid delegated role name " + role_name);
    }
    role_ = RoleEnum::kDelegated;
  } else if (role_name_lower == UPTANE_ROLE_ROOT) {
    role_ = RoleEnum::kRoot;
  } else if (role_name_lower == UPTANE_ROLE_SNAPSHOT) {
    role_ = RoleEnum::kSnapshot;
  } else if (role_name_lower == UPTANE_ROLE_TARGETS) {
    role_ = RoleEnum::kTargets;
  } else if (role_name_lower == UPTANE_ROLE_TIMESTAMP) {
    role_ = RoleEnum::kTimestamp;
  } else {
    role_ = RoleEnum::kInvalidRole;
    name_ = "invalidrole";
  }
}

std::string Role::ToString() const { return name_; }

std::ostream &Uptane::operator<<(std::ostream &os, const Role &role) {
  os << role.ToString();
  return os;
}

std::string Version::RoleFileName(const Role &role) const {
  std::stringstream ss;
  if (version_ != Version::ANY_VERSION) {
    ss << version_ << ".";
  }
  ss << role.ToString() << ".json";
  return ss.str();
}
