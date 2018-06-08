#include "uptane/tuf.h"

#include <sstream>

using Uptane::Role;
using Uptane::Version;

Role::Role(const std::string &role_name) {
  std::string role_name_lower;
  std::transform(role_name.begin(), role_name.end(), std::back_inserter(role_name_lower), ::tolower);
  if (role_name_lower == "root") {
    role_ = RoleEnum::Root;
  } else if (role_name_lower == "snapshot") {
    role_ = RoleEnum::Snapshot;
  } else if (role_name_lower == "targets") {
    role_ = RoleEnum::Targets;
  } else if (role_name_lower == "timestamp") {
    role_ = RoleEnum::Timestamp;
  } else {
    role_ = RoleEnum::InvalidRole;
  }
}

std::string Role::ToString() const {
  switch (role_) {
    case RoleEnum::Root:
      return "root";
    case RoleEnum::Snapshot:
      return "snapshot";
    case RoleEnum::Targets:
      return "targets";
    case RoleEnum::Timestamp:
      return "timestamp";
    default:
      return "invalidrole";
  }
}

std::ostream &Uptane::operator<<(std::ostream &os, const Role &t) {
  os << t.ToString();
  return os;
}

std::string Version::RoleFileName(Role role) const {
  std::stringstream ss;
  if (version_ != Version::ANY_VERSION) {
    ss << version_ << ".";
  }
  ss << role.ToString() << ".json";
  return ss.str();
}
