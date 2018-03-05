
#include "logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

using Uptane::Root;

Root::Root(const std::string &repository, const Json::Value &json) : BaseMeta(json), policy_(kCheck) {
  if (!json.isObject() || !json["signed"].isMember("keys") || !json["signed"].isMember("roles")) {
    throw Uptane::InvalidMetadata(repository, "root", "missing keys/roles field");
  }
  Json::Value keys = json["signed"]["keys"];
  for (Json::ValueIterator it = keys.begin(); it != keys.end(); ++it) {
    std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException(repository, "Unsupported key type: " + (*it)["keytype"].asString());
    }
    KeyId keyid = it.key().asString();
    PublicKey key((*it)["keyval"]["public"].asString(), (*it)["keytype"].asString());
    keys_[keyid] = key;
  }

  Json::Value roles = json["signed"]["roles"];
  for (Json::ValueIterator it = roles.begin(); it != roles.end(); it++) {
    std::string role_name = it.key().asString();
    Role role = Role(role_name);
    if (role == Role::InvalidRole()) {
      LOG_WARNING << "Invalid role in root.json";
      LOG_TRACE << "Role name:" << role_name;
      LOG_TRACE << "root.json is:" << json;
      continue;
    }
    // Threshold
    int requiredThreshold = (*it)["threshold"].asInt();
    if (requiredThreshold < kMinSignatures) {
      LOG_DEBUG << "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
                << (int)kMinSignatures;
      throw IllegalThreshold(repository, "The role " + role.ToString() + " had an illegal signature threshold.");
    }
    if (kMaxSignatures < requiredThreshold) {
      LOG_DEBUG << "Failing with threshold for role " << role << " too large: " << (int)kMaxSignatures << " < "
                << requiredThreshold;
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

Json::Value Root::toJson() const {
  Json::Value res = BaseMeta::toJson();

  if (policy_ != kCheck) throw Uptane::InvalidMetadata("", "root", "json representation will be invalid");

  res["_type"] = "Root";
  res["consistent_snapshot"] = false;

  res["keys"] = Json::objectValue;
  std::map<KeyId, PublicKey>::const_iterator key_it;
  for (key_it = keys_.begin(); key_it != keys_.end(); key_it++) {
    res["keys"][key_it->first]["keytype"] = key_it->second.TypeString();
    res["keys"][key_it->first]["keyval"]["public"] = key_it->second.value;
  }
  // assuming that Root object has keys for all targets,
  //   should be a part of verification process
  res["roles"]["root"]["keyids"] = Json::arrayValue;
  res["roles"]["snapshot"]["keyids"] = Json::arrayValue;
  res["roles"]["targets"]["keyids"] = Json::arrayValue;
  res["roles"]["timestamp"]["keyids"] = Json::arrayValue;

  std::set<std::pair<Role, KeyId> >::const_iterator role_key_it;
  for (role_key_it = keys_for_role_.begin(); role_key_it != keys_for_role_.end(); role_key_it++)
    res["roles"][role_key_it->first.ToString()]["keyids"].append(role_key_it->second);

  std::map<Role, int>::const_iterator th_it = thresholds_for_role_.find(Role::Root());
  if (th_it != thresholds_for_role_.end()) res["roles"]["root"]["threshold"] = th_it->second;

  th_it = thresholds_for_role_.find(Role::Snapshot());
  if (th_it != thresholds_for_role_.end()) res["roles"]["snapshot"]["threshold"] = th_it->second;

  th_it = thresholds_for_role_.find(Role::Targets());
  if (th_it != thresholds_for_role_.end()) res["roles"]["targets"]["threshold"] = th_it->second;

  th_it = thresholds_for_role_.find(Role::Timestamp());
  if (th_it != thresholds_for_role_.end()) res["roles"]["timestamp"]["threshold"] = th_it->second;

  return res;
}

Json::Value Root::UnpackSignedObject(TimeStamp now, std::string repository, Role role,
                                     const Json::Value &signed_object) {
  if (policy_ == kAcceptAll) {
    return signed_object;
  } else if (policy_ == kRejectAll) {
    throw SecurityException(repository, "Root policy is kRejectAll");
  }
  assert(policy_ == kCheck);

  std::string canonical = Json::FastWriter().write(signed_object["signed"]);
  Json::Value signatures = signed_object["signatures"];
  int valid_signatures = 0;

  std::vector<std::string> sig_values;
  for (Json::ValueIterator sig = signatures.begin(); sig != signatures.end(); ++sig) {
    std::string signature = (*sig)["sig"].asString();
    if (std::find(sig_values.begin(), sig_values.end(), signature) != sig_values.end()) {
      throw NonUniqueSignatures(repository, role.ToString());
    } else {
      sig_values.push_back(signature);
    }

    std::string method((*sig)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss" && method != "rsassa-pss-sha256" && method != "ed25519") {
      throw SecurityException(repository, std::string("Unsupported sign method: ") + (*sig)["method"].asString());
    }
    std::string keyid = (*sig)["keyid"].asString();
    if (!keys_.count(keyid)) {
      LOG_INFO << "Signed by unknown keyid, skipping";
      LOG_TRACE << "Key ID: " << keyid;
      continue;
    }

    if (!keys_for_role_.count(std::make_pair(role, keyid))) {
      LOG_WARNING << "KeyId is not valid to sign for this role";
      LOG_TRACE << "KeyId: " << keyid;
      continue;
    }

    if (Crypto::VerifySignature(keys_[keyid], signature, canonical)) {
      valid_signatures++;
    } else {
      LOG_WARNING << "Signature was present but invalid";
      LOG_TRACE << "  keyid:" << keyid;
      LOG_TRACE << "  signature:" << signature;
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

  // TODO check _type matches role
  // TODO check timestamp
  Uptane::TimeStamp expiry(Uptane::TimeStamp(signed_object["signed"]["expires"].asString()));
  if (expiry.IsExpiredAt(now)) {
    LOG_TRACE << "Metadata expired at:" << expiry;
    throw ExpiredMetadata(repository, role.ToString());
  }

  Uptane::Role actual_role(Uptane::Role(signed_object["signed"]["_type"].asString()));
  if (role != actual_role) {
    LOG_WARNING << "Object was signed for a different role";
    LOG_TRACE << "  role:" << role;
    LOG_TRACE << "  actual_role:" << actual_role;
    throw SecurityException(repository, "Object was signed for a different role");
  }
  return signed_object;
}
