#include "uptane/tuf.h"

#include <sstream>

using Uptane::Role;
using Uptane::Version;

Role::Role(const std::string &role_name, const bool delegation) {
  std::string role_name_lower;
  std::transform(role_name.begin(), role_name.end(), std::back_inserter(role_name_lower), ::tolower);
  name_ = role_name_lower;
  if (role_name_lower == "root") {
    role_ = RoleEnum::kRoot;
  } else if (role_name_lower == "snapshot") {
    role_ = RoleEnum::kSnapshot;
  } else if (role_name_lower == "targets") {
    role_ = RoleEnum::kTargets;
  } else if (role_name_lower == "timestamp") {
    role_ = RoleEnum::kTimestamp;
  } else if (delegation) {
    role_ = RoleEnum::kDelegated;
    name_ = role_name;
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
