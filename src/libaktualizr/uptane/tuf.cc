#include "uptane/tuf.h"

#include <ctime>
#include <ostream>
#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <utility>

#include "crypto/crypto.h"
#include "logging/logging.h"
#include "utilities/exceptions.h"

using Uptane::Target;
using Uptane::Version;

std::ostream &Uptane::operator<<(std::ostream &os, const Version &v) {
  if (v.version_ == Version::ANY_VERSION) {
    os << "vANY";
  } else {
    os << "v" << v.version_;
  }
  return os;
}

std::ostream &Uptane::operator<<(std::ostream &os, const HardwareIdentifier &hwid) {
  os << hwid.hwid_;
  return os;
}

std::ostream &Uptane::operator<<(std::ostream &os, const EcuSerial &ecu_serial) {
  os << ecu_serial.ecu_serial_;
  return os;
}

std::string Hash::encodeVector(const std::vector<Hash> &hashes) {
  std::stringstream hs;

  for (auto it = hashes.cbegin(); it != hashes.cend(); it++) {
    hs << it->TypeString() << ":" << it->HashString();
    if (std::next(it) != hashes.cend()) {
      hs << ";";
    }
  }

  return hs.str();
}

std::vector<Hash> Hash::decodeVector(std::string hashes_str) {
  std::vector<Hash> hash_v;

  std::string cs = std::move(hashes_str);
  while (!cs.empty()) {
    size_t scp = cs.find(';');
    std::string hash_token = cs.substr(0, scp);
    if (scp == std::string::npos) {
      cs = "";
    } else {
      cs = cs.substr(scp + 1);
    }
    if (hash_token.empty()) {
      break;
    }

    size_t cp = hash_token.find(':');
    std::string hash_type_str = hash_token.substr(0, cp);
    if (cp == std::string::npos) {
      break;
    }
    std::string hash_value_str = hash_token.substr(cp + 1);

    if (!hash_value_str.empty()) {
      Hash h{hash_type_str, hash_value_str};
      if (h.type() != Hash::Type::kUnknownAlgorithm) {
        hash_v.push_back(std::move(h));
      }
    }
  }

  return hash_v;
}

Target::Target(std::string filename, const Json::Value &content) : filename_(std::move(filename)) {
  if (content.isMember("custom")) {
    custom_ = content["custom"];

    // Image repo provides an array of hardware IDs.
    if (custom_.isMember("hardwareIds")) {
      Json::Value hwids = custom_["hardwareIds"];
      for (auto i = hwids.begin(); i != hwids.end(); ++i) {
        hwids_.emplace_back(HardwareIdentifier((*i).asString()));
      }
    }

    // Director provides a map of ECU serials to hardware IDs.
    Json::Value ecus = custom_["ecuIdentifiers"];
    for (auto i = ecus.begin(); i != ecus.end(); ++i) {
      ecus_.insert({EcuSerial(i.key().asString()), HardwareIdentifier((*i)["hardwareId"].asString())});
    }

    if (custom_.isMember("targetFormat")) {
      type_ = custom_["targetFormat"].asString();
    }

    if (custom_.isMember("uri")) {
      std::string custom_uri = custom_["uri"].asString();
      // Ignore this exact URL for backwards compatibility with old defaults that inserted it.
      if (custom_uri != "https://example.com/") {
        uri_ = std::move(custom_uri);
      }
    }
  }

  length_ = content["length"].asUInt64();

  Json::Value hashes = content["hashes"];
  for (auto i = hashes.begin(); i != hashes.end(); ++i) {
    Hash h(i.key().asString(), (*i).asString());
    if (h.HaveAlgorithm()) {
      hashes_.push_back(h);
    }
  }
  // sort hashes so that higher priority hash algorithm goes first
  std::sort(hashes_.begin(), hashes_.end(), [](const Hash &l, const Hash &r) { return l.type() < r.type(); });
}

// Internal use only.
Target::Target(std::string filename, EcuMap ecus, std::vector<Hash> hashes, uint64_t length, std::string correlation_id)
    : filename_(std::move(filename)),
      ecus_(std::move(ecus)),
      hashes_(std::move(hashes)),
      length_(length),
      correlation_id_(std::move(correlation_id)) {
  // sort hashes so that higher priority hash algorithm goes first
  std::sort(hashes_.begin(), hashes_.end(), [](const Hash &l, const Hash &r) { return l.type() < r.type(); });
  type_ = "UNKNOWN";
}

Target Target::Unknown() {
  Json::Value t_json;
  t_json["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
  t_json["length"] = 0;
  Uptane::Target target{"unknown", t_json};

  target.valid = false;

  return target;
}

bool Target::MatchHash(const Hash &hash) const {
  return (std::find(hashes_.begin(), hashes_.end(), hash) != hashes_.end());
}

std::string Target::hashString(Hash::Type type) const {
  std::vector<Hash>::const_iterator it;
  for (it = hashes_.begin(); it != hashes_.end(); it++) {
    if (it->type() == type) {
      return boost::algorithm::to_lower_copy(it->HashString());
    }
  }
  return std::string("");
}

std::string Target::sha256Hash() const { return hashString(Hash::Type::kSha256); }

std::string Target::sha512Hash() const { return hashString(Hash::Type::kSha512); }

bool Target::IsOstree() const {
  if (type_ == "OSTREE") {
    // Modern servers explicitly specify the type of the target
    return true;
  } else if (type_.empty() && length() == 0) {
    // Older servers don't specify the type of the target. Assume that it is
    // an OSTree target if the length is zero.
    return true;
  } else {
    // If type is explicitly not OSTREE or the length is non-zero, then this
    // is a firmware blob.
    return false;
  }
}

bool Target::MatchTarget(const Target &t2) const {
  // type_ (targetFormat) is only provided by the Image repo.
  // ecus_ is only provided by the Image repo.
  // correlation_id_ is only provided by the Director.
  // uri_ is not matched. If the Director provides it, we use that. If not, but
  // the Image repository does, use that. Otherwise, leave it empty and use the
  // default.
  if (filename_ != t2.filename_) {
    return false;
  }
  if (length_ != t2.length_) {
    return false;
  }

  // If the HWID vector and ECU->HWID map match, we're good. Otherwise, assume
  // we have a Target from the Director (ECU->HWID map populated, HWID vector
  // empty) and a Target from the Image repo (HWID vector populated,
  // ECU->HWID map empty). Figure out which Target has the map, and then for
  // every item in the map, make sure it's in the other Target's HWID vector.
  if (hwids_ != t2.hwids_ || ecus_ != t2.ecus_) {
    std::shared_ptr<EcuMap> ecu_map;                               // Director
    std::shared_ptr<std::vector<HardwareIdentifier>> hwid_vector;  // Image repo
    if (!hwids_.empty() && ecus_.empty() && t2.hwids_.empty() && !t2.ecus_.empty()) {
      ecu_map = std::make_shared<EcuMap>(t2.ecus_);
      hwid_vector = std::make_shared<std::vector<HardwareIdentifier>>(hwids_);
    } else if (!t2.hwids_.empty() && t2.ecus_.empty() && hwids_.empty() && !ecus_.empty()) {
      ecu_map = std::make_shared<EcuMap>(ecus_);
      hwid_vector = std::make_shared<std::vector<HardwareIdentifier>>(t2.hwids_);
    } else {
      return false;
    }
    for (auto map_it = ecu_map->cbegin(); map_it != ecu_map->cend(); ++map_it) {
      auto vec_it = find(hwid_vector->cbegin(), hwid_vector->cend(), map_it->second);
      if (vec_it == hwid_vector->end()) {
        return false;
      }
    }
  }

  // requirements:
  // - all hashes of the same type should match
  // - at least one pair of hashes should match
  bool oneMatchingHash = false;
  for (const Hash &hash : hashes_) {
    for (const Hash &hash2 : t2.hashes_) {
      if (hash.type() == hash2.type() && !(hash == hash2)) {
        return false;
      }
      if (hash == hash2) {
        oneMatchingHash = true;
      }
    }
  }
  return oneMatchingHash;
}

Json::Value Target::toDebugJson() const {
  Json::Value res;
  for (const auto &ecu : ecus_) {
    res["custom"]["ecuIdentifiers"][ecu.first.ToString()]["hardwareId"] = ecu.second.ToString();
  }
  if (!hwids_.empty()) {
    Json::Value hwids;
    for (Json::Value::ArrayIndex i = 0; i < hwids_.size(); ++i) {
      hwids[i] = hwids_[i].ToString();
    }
    res["custom"]["hardwareIds"] = hwids;
  }
  res["custom"]["targetFormat"] = type_;

  for (const auto &hash : hashes_) {
    res["hashes"][hash.TypeString()] = hash.HashString();
  }
  res["length"] = Json::Value(static_cast<Json::Value::Int64>(length_));
  return res;
}

std::ostream &Uptane::operator<<(std::ostream &os, const Target &t) {
  os << "Target(" << t.filename_;
  os << " ecu_identifiers: (";
  for (const auto &ecu : t.ecus_) {
    os << ecu.first << " (hw_id: " << ecu.second << "), ";
  }
  os << ")"
     << " hw_ids: (";
  for (const auto &hwid : t.hwids_) {
    os << hwid << ", ";
  }
  os << ")"
     << " length:" << t.length();
  os << " hashes: (";
  for (const auto &hash : t.hashes_) {
    os << hash << ", ";
  }
  os << "))";

  return os;
}

void Uptane::BaseMeta::init(const Json::Value &json) {
  if (!json.isObject() || !json.isMember("signed")) {
    LOG_ERROR << "Failure during base metadata initialization from json";
    throw Uptane::InvalidMetadata("", "", "invalid metadata json");
  }

  version_ = json["signed"]["version"].asInt();
  try {
    expiry_ = TimeStamp(json["signed"]["expires"].asString());
  } catch (const TimeStamp::InvalidTimeStamp &exc) {
    throw Uptane::InvalidMetadata("", "", "invalid timestamp");
  }
  original_object_ = json;
}
Uptane::BaseMeta::BaseMeta(const Json::Value &json) { init(json); }

Uptane::BaseMeta::BaseMeta(RepositoryType repo, const Role &role, const Json::Value &json,
                           const std::shared_ptr<MetaWithKeys> &signer) {
  if (!json.isObject() || !json.isMember("signed")) {
    throw Uptane::InvalidMetadata("", "", "invalid metadata json");
  }

  signer->UnpackSignedObject(repo, role, json);

  init(json);
}

void Uptane::Targets::init(const Json::Value &json) {
  if (!json.isObject() || json["signed"]["_type"] != "Targets") {
    throw Uptane::InvalidMetadata("", "targets", "invalid targets.json");
  }

  const Json::Value target_list = json["signed"]["targets"];
  for (auto t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    Target t(t_it.key().asString(), *t_it);
    targets.push_back(t);
  }

  if (json["signed"]["delegations"].isObject()) {
    const Json::Value key_list = json["signed"]["delegations"]["keys"];
    ParseKeys(Uptane::RepositoryType::Image(), key_list);

    const Json::Value role_list = json["signed"]["delegations"]["roles"];
    for (auto it = role_list.begin(); it != role_list.end(); it++) {
      const std::string role_name = (*it)["name"].asString();
      const Role role = Role::Delegation(role_name);
      delegated_role_names_.push_back(role_name);
      ParseRole(Uptane::RepositoryType::Image(), it, role, name_);

      const Json::Value paths_list = (*it)["paths"];
      std::vector<std::string> paths;
      for (auto p_it = paths_list.begin(); p_it != paths_list.end(); p_it++) {
        paths.emplace_back((*p_it).asString());
      }
      paths_for_role_[role] = paths;

      terminating_role_[role] = (*it)["terminating"].asBool();
    }
  }

  if (json["signed"]["custom"].isObject()) {
    correlation_id_ = json["signed"]["custom"]["correlationId"].asString();
  } else {
    correlation_id_ = "";
  }
}

Uptane::Targets::Targets(const Json::Value &json) : MetaWithKeys(json) { init(json); }

Uptane::Targets::Targets(RepositoryType repo, const Role &role, const Json::Value &json,
                         const std::shared_ptr<MetaWithKeys> &signer)
    : MetaWithKeys(repo, role, json, signer), name_(role.ToString()) {
  init(json);
}

void Uptane::TimestampMeta::init(const Json::Value &json) {
  Json::Value hashes_list = json["signed"]["meta"]["snapshot.json"]["hashes"];
  Json::Value meta_size = json["signed"]["meta"]["snapshot.json"]["length"];
  Json::Value meta_version = json["signed"]["meta"]["snapshot.json"]["version"];
  if (!json.isObject() || json["signed"]["_type"] != "Timestamp" || !hashes_list.isObject() ||
      !meta_size.isIntegral() || !meta_version.isIntegral()) {
    throw Uptane::InvalidMetadata("", "timestamp", "invalid timestamp.json");
  }

  for (auto it = hashes_list.begin(); it != hashes_list.end(); ++it) {
    Hash h(it.key().asString(), (*it).asString());
    snapshot_hashes_.push_back(h);
  }
  snapshot_size_ = meta_size.asInt();
  snapshot_version_ = meta_version.asInt();
}

Uptane::TimestampMeta::TimestampMeta(const Json::Value &json) : BaseMeta(json) { init(json); }

Uptane::TimestampMeta::TimestampMeta(RepositoryType repo, const Json::Value &json,
                                     const std::shared_ptr<MetaWithKeys> &signer)
    : BaseMeta(repo, Role::Timestamp(), json, signer) {
  init(json);
}

void Uptane::Snapshot::init(const Json::Value &json) {
  Json::Value meta_list = json["signed"]["meta"];
  if (!json.isObject() || json["signed"]["_type"] != "Snapshot" || !meta_list.isObject()) {
    throw Uptane::InvalidMetadata("", "snapshot", "invalid snapshot.json");
  }

  for (auto it = meta_list.begin(); it != meta_list.end(); ++it) {
    Json::Value hashes_list = (*it)["hashes"];
    Json::Value meta_size = (*it)["length"];
    Json::Value meta_version = (*it)["version"];

    if (!meta_version.isIntegral()) {
      throw Uptane::InvalidMetadata("", "snapshot", "invalid snapshot.json");
    }

    auto role_name =
        it.key().asString().substr(0, it.key().asString().rfind('.'));  // strip extension from the role name
    auto role_object = Role(role_name, !Role::IsReserved(role_name));

    if (meta_version.isIntegral()) {
      role_version_[role_object] = meta_version.asInt();
    } else {
      role_version_[role_object] = -1;
    }

    // Size and hashes are not required, but we may as well record them if
    // present.
    if (meta_size.isObject()) {
      role_size_[role_object] = meta_size.asInt64();
    } else {
      role_size_[role_object] = -1;
    }
    if (hashes_list.isObject()) {
      for (auto h_it = hashes_list.begin(); h_it != hashes_list.end(); ++h_it) {
        Hash h(h_it.key().asString(), (*h_it).asString());
        role_hashes_[role_object].push_back(h);
      }
    }
  }
}

Uptane::Snapshot::Snapshot(const Json::Value &json) : BaseMeta(json) { init(json); }

Uptane::Snapshot::Snapshot(RepositoryType repo, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &signer)
    : BaseMeta(repo, Role::Snapshot(), json, signer) {
  init(json);
}

std::vector<Hash> Uptane::Snapshot::role_hashes(const Uptane::Role &role) const {
  auto hashes = role_hashes_.find(role);
  if (hashes == role_hashes_.end()) {
    return std::vector<Hash>();
  } else {
    return hashes->second;
  }
}

int64_t Uptane::Snapshot::role_size(const Uptane::Role &role) const {
  auto size = role_size_.find(role);
  if (size == role_size_.end()) {
    return 0;
  } else {
    return size->second;
  }
}

int Uptane::Snapshot::role_version(const Uptane::Role &role) const {
  auto version = role_version_.find(role);
  if (version == role_version_.end()) {
    return -1;
  } else {
    return version->second;
  }
};

int Uptane::extractVersionUntrusted(const std::string &meta) {
  auto version_json = Utils::parseJSON(meta)["signed"]["version"];
  if (!version_json.isIntegral()) {
    return -1;
  } else {
    return version_json.asInt();
  }
}

std::string Uptane::getMetaFromBundle(const MetaBundle &bundle, const RepositoryType repo, const Role &role) {
  auto it = bundle.find(std::make_pair(repo, role));
  if (it == bundle.end()) {
    throw std::runtime_error("Metadata not found for " + role.ToString() + " role from the " + repo.toString() +
                             " repository.");
  }
  return it->second;
}
