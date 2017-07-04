#include "uptane/tuf.h"

#include <sstream>

using Uptane::Role;
using Uptane::Version;

Role::Role(const std::string &role_name) {
  std::string role_name_lower;
  std::transform(role_name.begin(), role_name.end(), std::back_inserter(role_name_lower), ::tolower);
  if (role_name_lower == "root") {
    role_ = kRoot;
  } else if (role_name_lower == "snapshot") {
    role_ = kSnapshot;
  } else if (role_name_lower == "targets") {
    role_ = kTargets;
  } else if (role_name_lower == "timestamp") {
    role_ = kTimestamp;
  } else {
    role_ = kInvalidRole;
  }
}

std::string Role::ToString() const {
  switch (role_) {
    case kRoot:
      return "root";
    case kSnapshot:
      return "snapshot";
    case kTargets:
      return "targets";
    case kTimestamp:
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