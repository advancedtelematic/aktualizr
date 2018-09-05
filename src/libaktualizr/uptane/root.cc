
#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

using Uptane::Root;

Root::Root(const RepositoryType repo, const Json::Value &json, Root &root) : Root(repo, json) {
  root.UnpackSignedObject(repo, json);
  this->UnpackSignedObject(repo, json);
}

Root::Root(const RepositoryType repo, const Json::Value &json) : policy_(Policy::kCheck) {
  if (!json.isObject() || !json.isMember("signed")) {
    throw InvalidMetadata("", "", "invalid metadata json");
  }

  version_ = json["signed"]["version"].asInt();

  expiry_ = Uptane::TimeStamp(json["signed"]["expires"].asString());
  original_object_ = json;

  if (!json.isObject() || !json["signed"].isMember("keys") || !json["signed"].isMember("roles")) {
    throw InvalidMetadata(RepoString(repo), "root", "missing keys/roles field");
  }

  const Json::Value keys = json["signed"]["keys"];
  for (Json::ValueIterator it = keys.begin(); it != keys.end(); ++it) {
    const std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException(RepoString(repo), "Unsupported key type: " + (*it)["keytype"].asString());
    }
    const KeyId keyid = it.key().asString();
    PublicKey key(*it);
    keys_[keyid] = key;
  }

  const Json::Value roles = json["signed"]["roles"];
  for (Json::ValueIterator it = roles.begin(); it != roles.end(); it++) {
    const std::string role_name = it.key().asString();
    const Role role = Role(role_name);
    if (role == Role::InvalidRole()) {
      LOG_WARNING << "Invalid role in root.json";
      LOG_TRACE << "Role name:" << role_name;
      LOG_TRACE << "root.json is:" << json;
      continue;
    }
    // Threshold
    const int64_t requiredThreshold = (*it)["threshold"].asInt64();
    if (requiredThreshold < kMinSignatures) {
      // static_cast<int64_t> is to stop << taking a reference to kMinSignatures
      // http://www.stroustrup.com/bs_faq2.html#in-class
      // this occurs in Boost 1.62 (and possibly other versions)
      LOG_DEBUG << "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
                << static_cast<int64_t>(kMinSignatures);
      throw IllegalThreshold(RepoString(repo), "The role " + role_name + " had an illegal signature threshold.");
    }
    if (kMaxSignatures < requiredThreshold) {
      // static_cast<int> is to stop << taking a reference to kMaxSignatures
      // http://www.stroustrup.com/bs_faq2.html#in-class
      // this occurs in Boost 1.62  (and possibly other versions)
      LOG_DEBUG << "Failing with threshold for role " << role << " too large: " << static_cast<int>(kMaxSignatures)
                << " < " << requiredThreshold;
      throw IllegalThreshold(RepoString(repo), "root.json contains a role that requires too many signatures");
    }
    thresholds_for_role_[role] = requiredThreshold;

    // KeyIds
    const Json::Value keyids = (*it)["keyids"];
    for (Json::ValueIterator itk = keyids.begin(); itk != keyids.end(); ++itk) {
      keys_for_role_.insert(std::make_pair(role, (*itk).asString()));
    }
  }
}

void Uptane::Root::UnpackSignedObject(const RepositoryType repo, const Json::Value &signed_object) {
  const std::string repository = RepoString(repo);

  const Uptane::Role role(signed_object["signed"]["_type"].asString());
  if (policy_ == Policy::kAcceptAll) {
    return;
  }
  if (policy_ == Policy::kRejectAll) {
    throw SecurityException(repository, "Root policy is Policy::kRejectAll");
  }
  assert(policy_ == Policy::kCheck);

  const std::string canonical = Json::FastWriter().write(signed_object["signed"]);
  const Json::Value signatures = signed_object["signatures"];
  int valid_signatures = 0;

  std::set<std::string> used_keyids;
  for (Json::ValueIterator sig = signatures.begin(); sig != signatures.end(); ++sig) {
    const std::string keyid = (*sig)["keyid"].asString();
    if (used_keyids.count(keyid) != 0) {
      throw NonUniqueSignatures(repository, role.ToString());
    }
    used_keyids.insert(keyid);

    std::string method((*sig)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss" && method != "rsassa-pss-sha256" && method != "ed25519") {
      throw SecurityException(repository, std::string("Unsupported sign method: ") + (*sig)["method"].asString());
    }

    if (keys_.count(keyid) == 0u) {
      LOG_DEBUG << "Signed by unknown KeyId: " << keyid << ". Skipping.";
      continue;
    }

    if (keys_for_role_.count(std::make_pair(role, keyid)) == 0u) {
      LOG_WARNING << "KeyId " << keyid << " is not valid to sign for this role (" << role.ToString() << ").";
      continue;
    }
    const std::string signature = (*sig)["sig"].asString();
    if (keys_[keyid].VerifySignature(signature, canonical)) {
      valid_signatures++;
    } else {
      LOG_WARNING << "Signature was present but invalid: " << signature << " with KeyId: " << keyid;
    }
  }
  const int64_t threshold = thresholds_for_role_[role];
  if (threshold < kMinSignatures || kMaxSignatures < threshold) {
    throw IllegalThreshold(repository, "Invalid signature threshold");
  }
  // One signature and it is bad: throw bad key ID.
  // Multiple signatures but not enough good ones to pass threshold: throw unmet threshold.
  if (signatures.size() == 1 && valid_signatures == 0) {
    throw BadKeyId(repository);
  }
  if (valid_signatures < threshold) {
    throw UnmetThreshold(repository, role.ToString());
  }

  const Uptane::Role actual_role(Uptane::Role(signed_object["signed"]["_type"].asString()));
  if (role != actual_role) {
    LOG_ERROR << "Object was signed for a different role";
    LOG_TRACE << "  role:" << role;
    LOG_TRACE << "  actual_role:" << actual_role;
    throw SecurityException(repository, "Object was signed for a different role");
  }
}
