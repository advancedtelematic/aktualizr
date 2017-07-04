
#include "logger.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

using Uptane::Root;

Root::Root(const std::string &repository, const Json::Value &json) : policy_(kCheck) {
  if (!json.isObject() || !json.isMember("keys") || !json.isMember("roles")) {
    throw Uptane::InvalidMetadata(repository, "root", "missing keys/roles field");
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
    Role role = Role(role_name);
    if (role == Role::InvalidRole()) {
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
      throw IllegalThreshold(repository, "The role " + role.ToString() + " had an illegal signature threshold.");
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
      continue;
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
    throw UnmetThreshold(repository, role.ToString());
  }
  Json::Value res = Utils::parseJSON(canonical);

  // TODO check _type matches role
  // TODO check timestamp
  Uptane::TimeStamp expiry(Uptane::TimeStamp(res["expires"].asString()));
  if (expiry.IsExpiredAt(now)) {
    LOGGER_LOG(LVL_trace, "Metadata expired at:" << expiry);
    throw ExpiredMetadata(repository, role.ToString());
  }

  Uptane::Role actual_role(Uptane::Role(res["_type"].asString()));
  if (role != actual_role) {
    LOGGER_LOG(LVL_warning, "Object was signed for a different role");
    LOGGER_LOG(LVL_trace, "  role:" << role);
    LOGGER_LOG(LVL_trace, "  actual_role:" << actual_role);
    throw SecurityException(repository, "Object was signed for a different role");
  }
  return res;
}
