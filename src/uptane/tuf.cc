#include <logger.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <ctime>
#include <ostream>
#include <sstream>

#include "crypto.h"
#include "exceptions.h"
#include "uptane/tuf.h"

using Uptane::Hash;
using Uptane::Target;
using Uptane::Root;
using Uptane::Role;
using Uptane::TimeStamp;
using Uptane::Version;

std::string Version::RoleFileName(Role role) const {
  std::stringstream ss;
  if (version_ != Version::ANY_VERSION) {
    ss << version_ << ".";
  }
  ss << StringFromRole(role) << ".json";
  return ss.str();
}

std::ostream &Uptane::operator<<(std::ostream &os, const Version &v) {
  if (v.version_ == Version::ANY_VERSION) {
    os << "vANY";
  } else {
    os << "v" << v.version_;
  }
  return os;
}

TimeStamp TimeStamp::Now() {
  time_t raw_time;
  struct tm time_struct;
  time(&raw_time);
  gmtime_r(&raw_time, &time_struct);
  char formatted[22];
  strftime(formatted, 22, "%Y-%m-%dT%H:%M:%SZ", &time_struct);
  return TimeStamp(formatted);
}

TimeStamp::TimeStamp(std::string rfc3339) {
  if (rfc3339.length() != 20 || rfc3339[19] != 'Z') {
    time_ = "";
  } else {
    time_ = rfc3339;
  }
}

bool TimeStamp::valid() const { return time_.length() != 0; }

bool TimeStamp::operator<(const TimeStamp &other) const { return valid() && other.valid() && time_ < other.time_; }

bool TimeStamp::operator>(const TimeStamp &other) const { return (other < *this); }

std::ostream &Uptane::operator<<(std::ostream &os, const TimeStamp &t) {
  os << t.time_;
  return os;
}

Role Uptane::RoleFromString(const std::string &rolename) {
  if (rolename == "root" || rolename == "Root") {
    return Uptane::kRoot;
  } else if (rolename == "snapshot" || rolename == "Snapshot") {
    return Uptane::kSnapshot;
  } else if (rolename == "targets" || rolename == "Targets") {
    return Uptane::kTargets;
  } else if (rolename == "timestamp" || rolename == "Timestamp") {
    return Uptane::kTimestamp;
  } else {
    return Uptane::kInvalidRole;
  }
}

std::string Uptane::StringFromRole(Role role) {
  switch (role) {
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
Hash::Hash(Type type, const std::string &hash) : type_(type), hash_(boost::algorithm::to_upper_copy(hash)) {}

bool Hash::matchWith(const std::string &content) const {
  switch (type_) {
    case sha256:
      return hash_ == boost::algorithm::hex(Crypto::sha256digest(content));
    case sha512:
      return hash_ == boost::algorithm::hex(Crypto::sha512digest(content));
    default:
      throw std::invalid_argument("type_");  // Unreachable
  }
}

bool Hash::operator==(const Hash &other) const { return type_ == other.type_ && hash_ == other.hash_; }

std::ostream &Uptane::operator<<(std::ostream &os, const Target &t) {
  os << "Target(" << t.filename_ << " ecu_identifier:" << t.ecu_identifier() << " length:" << t.length() << ")";
  return os;
}

Root::Root(const std::string &repository, const Json::Value &json) : policy_(kCheck) {
  if (!json.isObject()) {
    LOGGER_LOG(LVL_error, "Trying to construct Tuf Root from invalid json");
    LOGGER_LOG(LVL_trace, json);
    policy_ = kRejectAll;
    return;
  }
  Json::Value keys = json["keys"];
  for (Json::ValueIterator it = keys.begin(); it != keys.end(); ++it) {
    std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException(repository, "Unsupported key type: " + (*it)["keytype"].asString());
    }
    KeyId keyid = it.key().asString();
    PublicKey key((*it)["keyval"]["public"].asString(), (*it)["keytype"].asString());
    keys_[keyid] = key;
  }

  Json::Value roles = json["roles"];
  for (Json::ValueIterator it = roles.begin(); it != roles.end(); it++) {
    std::string role_name = it.key().asString();
    Role role = RoleFromString(role_name);
    if (role == kInvalidRole) {
      LOGGER_LOG(LVL_warning, "Invalid role in root.json");
      LOGGER_LOG(LVL_trace, "Role name:" << role_name);
      LOGGER_LOG(LVL_trace, "root.json is:" << json);
      continue;
    }
    // Threshold
    int requiredThreshold = (*it)["threshold"].asInt();
    if (requiredThreshold < kMinSignatures) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
                                                               << kMinSignatures);
      throw IllegalThreshold(repository, "The role " + StringFromRole(role) + " had an illegal signature threshold.");
    }
    if (kMaxSignatures < requiredThreshold) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too large: " << kMaxSignatures << " < "
                                                               << requiredThreshold);
      throw IllegalThreshold(repository, "root.json contains a role that requires too many signatures");
    }
    thresholds_for_role_[role] = requiredThreshold;

    // KeyIds
    Json::Value keyids = (*it)["keyids"];
    for (Json::ValueIterator itk = keyids.begin(); itk != keyids.end(); ++itk) {
      keys_for_role_.insert(std::make_pair(role, (*itk).asString()));
    }
  }
}

Json::Value Root::UnpackSignedObject(TimeStamp now, std::string repository, Role role,
                                     const Json::Value &signed_object) {
  if (policy_ == kAcceptAll) {
    return signed_object["signed"];
  } else if (policy_ == kRejectAll) {
    throw SecurityException(repository, "Root policy is kRejectAll");
  }
  assert(policy_ == kCheck);

  std::string canonical = Json::FastWriter().write(signed_object["signed"]);
  Json::Value signatures = signed_object["signatures"];
  int valid_signatures = 0;

  for (Json::ValueIterator sig = signatures.begin(); sig != signatures.end(); ++sig) {
    std::string method((*sig)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss" && method != "rsassa-pss-sha256" && method != "ed25519") {
      throw SecurityException(repository, std::string("Unsupported sign method: ") + (*sig)["method"].asString());
    }
    std::string keyid = (*sig)["keyid"].asString();
    if (!keys_.count(keyid)) {
      LOGGER_LOG(LVL_info, "Signed by unknown keyid, skipping");
      LOGGER_LOG(LVL_trace, "Key ID: " << keyid);
      continue;
    }

    if (!keys_for_role_.count(std::make_pair(role, keyid))) {
      LOGGER_LOG(LVL_warning, "KeyId is not valid to sign for this role");
      LOGGER_LOG(LVL_trace, "KeyId: " << keyid);
    }

    std::string signature = (*sig)["sig"].asString();
    if (Crypto::VerifySignature(keys_[keyid], signature, canonical)) {
      valid_signatures++;
    } else {
      LOGGER_LOG(LVL_warning, "Signature was present but invalid");
      LOGGER_LOG(LVL_trace, "  keyid:" << keyid);
      LOGGER_LOG(LVL_trace, "  signature:" << signature);
      // throw SecurityException(repository, "Invalid signature, verification failed");
    }
  }
  int threshold = thresholds_for_role_[role];
  if (threshold < kMinSignatures || kMaxSignatures < threshold) {
    throw IllegalThreshold(repository, "Invalid signature threshold");
  }
  if (valid_signatures < threshold) {
    throw UnmetThreshold(repository, StringFromRole(role));
  }
  Json::Value res = Utils::parseJSON(canonical);

  // TODO check _type matches role
  // TODO check timestamp
  Uptane::TimeStamp expiry(Uptane::TimeStamp(res["expires"].asString()));
  if (!expiry.valid() || expiry < TimeStamp::Now()) {
    throw ExpiredMetadata(repository, StringFromRole(role));
  }

  Uptane::Role actual_role(Uptane::RoleFromString(res["_type"].asString()));
  if (role != actual_role) {
    LOGGER_LOG(LVL_warning, "Object was signed for a different role");
    LOGGER_LOG(LVL_trace, "  role:" << role);
    LOGGER_LOG(LVL_trace, "  actual_role:" << actual_role);
    throw SecurityException(repository, "Object was signed for a different role");
  }
  return res;
}
