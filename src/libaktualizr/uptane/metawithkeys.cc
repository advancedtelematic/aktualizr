#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

using Uptane::MetaWithKeys;

MetaWithKeys::MetaWithKeys(const Json::Value &json) : BaseMeta(json) {}
MetaWithKeys::MetaWithKeys(RepositoryType repo, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &root)
    : BaseMeta(repo, json, root) {}

void Uptane::MetaWithKeys::ParseKeys(const RepositoryType repo, const Json::Value &keys) {
  for (Json::ValueIterator it = keys.begin(); it != keys.end(); ++it) {
    const std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException(repo, "Unsupported key type: " + (*it)["keytype"].asString());
    }
    const KeyId keyid = it.key().asString();
    PublicKey key(*it);
    keys_[keyid] = key;
  }
}

void Uptane::MetaWithKeys::ParseRole(const RepositoryType repo, const Json::ValueIterator &it, const Role &role,
                                     const std::string &meta_role) {
  if (role == Role::InvalidRole()) {
    LOG_WARNING << "Invalid role in " << meta_role << ".json";
    LOG_TRACE << "Role name:" << role.ToString();
    return;
  }
  // Threshold
  const int64_t requiredThreshold = (*it)["threshold"].asInt64();
  if (requiredThreshold < kMinSignatures) {
    // static_cast<int64_t> is to stop << taking a reference to kMinSignatures
    // http://www.stroustrup.com/bs_faq2.html#in-class
    // this occurs in Boost 1.62 (and possibly other versions)
    LOG_DEBUG << "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
              << static_cast<int64_t>(kMinSignatures);
    throw IllegalThreshold(repo, "The role " + role.ToString() + " had an illegal signature threshold.");
  }
  if (kMaxSignatures < requiredThreshold) {
    // static_cast<int> is to stop << taking a reference to kMaxSignatures
    // http://www.stroustrup.com/bs_faq2.html#in-class
    // this occurs in Boost 1.62  (and possibly other versions)
    LOG_DEBUG << "Failing with threshold for role " << role << " too large: " << static_cast<int>(kMaxSignatures)
              << " < " << requiredThreshold;
    throw IllegalThreshold(repo, meta_role + ".json contains a role that requires too many signatures");
  }
  thresholds_for_role_[role] = requiredThreshold;

  // KeyIds
  const Json::Value keyids = (*it)["keyids"];
  for (Json::ValueIterator itk = keyids.begin(); itk != keyids.end(); ++itk) {
    keys_for_role_.insert(std::make_pair(role, (*itk).asString()));
  }
}

void Uptane::MetaWithKeys::UnpackSignedObject(const RepositoryType repo, const Json::Value &signed_object) {
  const std::string repository = repo;

  const std::string canonical = Json::FastWriter().write(signed_object["signed"]);
  const Uptane::Role role(signed_object["signed"]["_type"].asString());
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

  // TODO: fix for delegated targets.
  // Actually, just fix this for everything; it's broken.
  // It just checks the an object is equal to itself. role should come from the
  // roles of this object, not the signed_object.
  const Uptane::Role actual_role(Uptane::Role(signed_object["signed"]["_type"].asString()));
  if (role != actual_role) {
    LOG_ERROR << "Object was signed for a different role";
    LOG_TRACE << "  role:" << role;
    LOG_TRACE << "  actual_role:" << actual_role;
    throw SecurityException(repository, "Object was signed for a different role");
  }
}
