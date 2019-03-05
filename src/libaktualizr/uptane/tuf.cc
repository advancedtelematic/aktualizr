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

using Uptane::Hash;
using Uptane::MetaPack;
using Uptane::Root;
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

Hash::Hash(const std::string &type, const std::string &hash) : hash_(boost::algorithm::to_upper_copy(hash)) {
  if (type == "sha512") {
    type_ = Hash::Type::kSha512;
  } else if (type == "sha256") {
    type_ = Hash::Type::kSha256;
  } else {
    type_ = Hash::Type::kUnknownAlgorithm;
  }
}

Hash::Hash(Type type, const std::string &hash) : type_(type), hash_(boost::algorithm::to_upper_copy(hash)) {}

bool Hash::operator==(const Hash &other) const { return type_ == other.type_ && hash_ == other.hash_; }

std::string Hash::TypeString() const {
  switch (type_) {
    case Type::kSha256:
      return "sha256";
    case Type::kSha512:
      return "sha512";
    default:
      return "unknown";
  }
}

Hash::Type Hash::type() const { return type_; }

std::ostream &Uptane::operator<<(std::ostream &os, const Hash &h) {
  os << "Hash: " << h.hash_;
  return os;
}

std::string Hash::encodeVector(const std::vector<Uptane::Hash> &hashes) {
  std::stringstream hs;

  for (auto it = hashes.cbegin(); it != hashes.cend(); it++) {
    hs << it->TypeString() << ":" << it->HashString();
    if (std::next(it) != hashes.cend()) {
      hs << ";";
    }
  }

  return hs.str();
}

std::vector<Uptane::Hash> Hash::decodeVector(std::string hashes_str) {
  std::vector<Uptane::Hash> hash_v;

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
      Uptane::Hash h{hash_type_str, hash_value_str};
      if (h.type() != Uptane::Hash::Type::kUnknownAlgorithm) {
        hash_v.push_back(std::move(h));
      }
    }
  }

  return hash_v;
}

Target::Target(std::string filename, const Json::Value &content) : filename_(std::move(filename)) {
  if (content.isMember("custom")) {
    custom_ = content["custom"];

    Json::Value hwids = custom_["hardwareIds"];
    for (Json::ValueIterator i = hwids.begin(); i != hwids.end(); ++i) {
      hwids_.emplace_back(HardwareIdentifier((*i).asString()));
    }

    Json::Value ecus = custom_["ecuIdentifiers"];
    for (Json::ValueIterator i = ecus.begin(); i != ecus.end(); ++i) {
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
  for (Json::ValueIterator i = hashes.begin(); i != hashes.end(); ++i) {
    Hash h(i.key().asString(), (*i).asString());
    if (h.HaveAlgorithm()) {
      hashes_.push_back(h);
    }
  }
  // sort hashes so that higher priority hash algorithm goes first
  std::sort(hashes_.begin(), hashes_.end(), [](const Hash &l, const Hash &r) { return l.type() < r.type(); });
}

Target::Target(std::string filename, std::vector<Hash> hashes, uint64_t length, std::string correlation_id)
    : filename_(std::move(filename)),
      hashes_(std::move(hashes)),
      length_(length),
      correlation_id_(std::move(correlation_id)) {
  // sort hashes so that higher priority hash algorithm goes first
  std::sort(hashes_.begin(), hashes_.end(), [](const Hash &l, const Hash &r) { return l.type() < r.type(); });
}

Target Target::Unknown() {
  Json::Value t_json;
  t_json["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
  t_json["length"] = 0;
  Uptane::Target target{"unknown", t_json};

  target.valid = false;

  return target;
}

bool Target::MatchWith(const Hash &hash) const {
  return (std::find(hashes_.begin(), hashes_.end(), hash) != hashes_.end());
}

std::string Target::hashString(Hash::Type type) const {
  std::vector<Uptane::Hash>::const_iterator it;
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

Json::Value Target::toDebugJson() const {
  Json::Value res;
  for (auto it = ecus_.begin(); it != ecus_.cend(); ++it) {
    res["custom"]["ecuIdentifiers"][it->first.ToString()]["hardwareId"] = it->second.ToString();
  }
  res["custom"]["targetFormat"] = type_;

  for (auto it = hashes_.cbegin(); it != hashes_.cend(); ++it) {
    res["hashes"][it->TypeString()] = it->HashString();
  }
  res["length"] = Json::Value(static_cast<Json::Value::Int64>(length_));
  return res;
}

std::ostream &Uptane::operator<<(std::ostream &os, const Target &t) {
  os << "Target(" << t.filename_;
  os << " ecu_identifiers: (";

  for (auto it = t.ecus_.begin(); it != t.ecus_.end(); ++it) {
    os << it->first;
  }
  os << ")"
     << " length:" << t.length();
  os << " hashes: (";
  for (auto it = t.hashes_.begin(); it != t.hashes_.end(); ++it) {
    os << *it << ", ";
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
    throw Uptane::InvalidMetadata("", "", "Invalid timestamp");
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
  for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    Target t(t_it.key().asString(), *t_it);
    targets.push_back(t);
  }

  if (json["signed"]["delegations"].isObject()) {
    const Json::Value key_list = json["signed"]["delegations"]["keys"];
    ParseKeys(Uptane::RepositoryType::Image(), key_list);

    const Json::Value role_list = json["signed"]["delegations"]["roles"];
    for (Json::ValueIterator it = role_list.begin(); it != role_list.end(); it++) {
      const std::string role_name = (*it)["name"].asString();
      const Role role = Role::Delegation(role_name);
      delegated_role_names_.push_back(role_name);
      ParseRole(Uptane::RepositoryType::Image(), it, role, name_);

      const Json::Value paths_list = (*it)["paths"];
      std::vector<std::string> paths;
      for (Json::ValueIterator p_it = paths_list.begin(); p_it != paths_list.end(); p_it++) {
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

  for (Json::ValueIterator it = hashes_list.begin(); it != hashes_list.end(); ++it) {
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

  for (Json::ValueIterator it = meta_list.begin(); it != meta_list.end(); ++it) {
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
      for (Json::ValueIterator h_it = hashes_list.begin(); h_it != hashes_list.end(); ++h_it) {
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

bool MetaPack::isConsistent() const {
  TimeStamp now(TimeStamp::Now());
  try {
    if (director_root.original() != Json::nullValue) {
      Uptane::Root original_root(director_root);
      Uptane::Root new_root(RepositoryType::Director(), director_root.original(), new_root);
      if (director_targets.original() != Json::nullValue) {
        Uptane::Targets(RepositoryType::Director(), Role::Targets(), director_targets.original(),
                        std::make_shared<MetaWithKeys>(original_root));
      }
    }
  } catch (const std::logic_error &exc) {
    LOG_WARNING << "Inconsistent metadata: " << exc.what();
    return false;
  }
  return true;
}

int Uptane::extractVersionUntrusted(const std::string &meta) {
  auto version_json = Utils::parseJSON(meta)["signed"]["version"];
  if (!version_json.isIntegral()) {
    return -1;
  } else {
    return version_json.asInt();
  }
}
