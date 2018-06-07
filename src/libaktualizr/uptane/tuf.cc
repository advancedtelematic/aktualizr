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
using Uptane::Root;
using Uptane::Target;
using Uptane::TimeStamp;
using Uptane::Version;
using Uptane::MetaPack;

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
  struct tm time_struct {};
  time(&raw_time);
  gmtime_r(&raw_time, &time_struct);
  char formatted[22];
  strftime(formatted, 22, "%Y-%m-%dT%H:%M:%SZ", &time_struct);
  return TimeStamp(formatted);
}

TimeStamp::TimeStamp(std::string rfc3339) {
  if (rfc3339.length() != 20 || rfc3339[19] != 'Z') {
    throw Uptane::InvalidMetadata("", "", "Invalid timestamp");
  }
  time_ = rfc3339;
}

bool TimeStamp::IsValid() const { return time_.length() != 0; }

bool TimeStamp::IsExpiredAt(const TimeStamp &now) const {
  if (!IsValid()) {
    return true;
  }
  if (!now.IsValid()) {
    return true;
  }
  return *this < now;
}

bool TimeStamp::operator<(const TimeStamp &other) const { return IsValid() && other.IsValid() && time_ < other.time_; }

bool TimeStamp::operator>(const TimeStamp &other) const { return (other < *this); }

std::ostream &Uptane::operator<<(std::ostream &os, const TimeStamp &t) {
  os << t.time_;
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

Target::Target(std::string filename, const Json::Value &content) : filename_(std::move(filename)) {
  if (content.isMember("custom")) {
    Json::Value custom = content["custom"];

    Json::Value ecus = custom["ecuIdentifiers"];
    for (Json::ValueIterator i = ecus.begin(); i != ecus.end(); ++i) {
      ecus_.insert({EcuSerial(i.key().asString()), HardwareIdentifier((*i)["hardwareId"].asString())});
    }

    if (custom.isMember("targetFormat")) {
      type_ = custom["targetFormat"].asString();
    }
  }

  length_ = content["length"].asInt64();

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

bool Target::MatchWith(const Hash &hash) const {
  return (std::find(hashes_.begin(), hashes_.end(), hash) != hashes_.end());
}

std::string Target::sha256Hash() const {
  std::vector<Uptane::Hash>::const_iterator it;
  for (it = hashes_.begin(); it != hashes_.end(); it++) {
    if (it->type() == Hash::Type::kSha256) {
      return boost::algorithm::to_lower_copy(it->HashString());
    }
  }
  return std::string();
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
    LOG_ERROR << "BM FAILURE";
    throw Uptane::InvalidMetadata("", "", "invalid metadata json");
  }

  version_ = json["signed"]["version"].asInt();
  expiry_ = Uptane::TimeStamp(json["signed"]["expires"].asString());
  original_object_ = json;
}
Uptane::BaseMeta::BaseMeta(const Json::Value &json) { init(json); }

Uptane::BaseMeta::BaseMeta(RepositoryType repo, const Json::Value &json, Root &root) {
  if (!json.isObject() || !json.isMember("signed")) {
    throw Uptane::InvalidMetadata("", "", "invalid metadata json");
  }

  root.UnpackSignedObject(repo, json);

  init(json);
}

void Uptane::Targets::init(const Json::Value &json) {
  if (!json.isObject() || json["signed"]["_type"] != "Targets") {
    throw Uptane::InvalidMetadata("", "targets", "invalid targets.json");
  }

  Json::Value target_list = json["signed"]["targets"];
  for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    Target t(t_it.key().asString(), *t_it);
    targets.push_back(t);
  }
}

Uptane::Targets::Targets(const Json::Value &json) : BaseMeta(json) { init(json); }

Uptane::Targets::Targets(RepositoryType repo, const Json::Value &json, Root &root) : BaseMeta(repo, json, root) {
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

Uptane::TimestampMeta::TimestampMeta(RepositoryType repo, const Json::Value &json, Root &root)
    : BaseMeta(repo, json, root) {
  init(json);
}

void Uptane::Snapshot::init(const Json::Value &json) {
  Json::Value hashes_list = json["signed"]["meta"]["targets.json"]["hashes"];
  Json::Value meta_size = json["signed"]["meta"]["targets.json"]["length"];
  Json::Value meta_version = json["signed"]["meta"]["targets.json"]["version"];

  if (!json.isObject() || json["signed"]["_type"] != "Snapshot" || !meta_version.isIntegral()) {
    throw Uptane::InvalidMetadata("", "snapshot", "invalid snapshot.json");
  }

  if (hashes_list.isObject()) {
    for (Json::ValueIterator it = hashes_list.begin(); it != hashes_list.end(); ++it) {
      Hash h(it.key().asString(), (*it).asString());
      targets_hashes_.push_back(h);
    }
  }

  if (meta_size.isIntegral()) {
    targets_size_ = meta_size.asInt();
  } else {
    targets_size_ = -1;
  }
  targets_version_ = meta_version.asInt();
}

Uptane::Snapshot::Snapshot(const Json::Value &json) : BaseMeta(json) { init(json); }

Uptane::Snapshot::Snapshot(RepositoryType repo, const Json::Value &json, Root &root) : BaseMeta(repo, json, root) {
  init(json);
}

bool MetaPack::isConsistent() const {
  TimeStamp now(TimeStamp::Now());
  try {
    Uptane::Root original_root(director_root);
    Uptane::Root new_root(RepositoryType::Director, director_root.original(), new_root);
    Uptane::Targets(RepositoryType::Director, director_targets.original(), original_root);
  } catch (...) {
    return false;
  }
  return true;
}

std::string Uptane::RepoString(RepositoryType repo) {
  switch (repo) {
    case RepositoryType::Director:
      return "director";
    case RepositoryType::Images:
      return "images";
    default:
      return "";
  }
}

int Uptane::extractVersionUntrusted(const std::string &meta) {
  auto version_json = Utils::parseJSON(meta)["signed"]["version"];
  if (!version_json.isIntegral()) {
    return -1;
  } else {
    return version_json.asInt();
  }
}
