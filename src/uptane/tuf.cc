#include "uptane/tuf.h"

#include <ctime>
#include <ostream>
#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "crypto.h"
#include "exceptions.h"
#include "logging.h"

using Uptane::Hash;
using Uptane::Role;
using Uptane::Root;
using Uptane::Target;
using Uptane::TimeStamp;
using Uptane::Version;

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
    throw Uptane::InvalidMetadata("", "", "Invalid timestamp");
  } else {
    time_ = rfc3339;
  }
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

Hash::Hash(const std::string &type, const std::string &hash) : hash_(boost::algorithm::to_upper_copy(hash)) {
  if (type == "sha512") {
    type_ = Hash::kSha512;
  } else if (type == "sha256") {
    type_ = Hash::kSha256;
  } else {
    type_ = Hash::kUnknownAlgorithm;
  }
}

Hash::Hash(Type type, const std::string &hash) : type_(type), hash_(boost::algorithm::to_upper_copy(hash)) {}

bool Hash::operator==(const Hash &other) const { return type_ == other.type_ && hash_ == other.hash_; }

std::string Hash::TypeString() const {
  switch (type_) {
    case kSha256:
      return "sha256";
    case kSha512:
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

Target::Target(const std::string &filename, const Json::Value &content) : filename_(filename), ecu_identifier_("") {
  if (content.isMember("custom")) {
    Json::Value custom = content["custom"];
    // TODO: Hardware identifier?
    if (custom.isMember("ecuIdentifier")) ecu_identifier_ = custom["ecuIdentifier"].asString();
    if (custom.isMember("targetFormat")) type_ = custom["targetFormat"].asString();
  }

  length_ = content["length"].asInt64();

  Json::Value hashes = content["hashes"];
  for (Json::ValueIterator i = hashes.begin(); i != hashes.end(); ++i) {
    Hash h(i.key().asString(), (*i).asString());
    if (h.HaveAlgorithm()) {
      hashes_.push_back(h);
    }
  }
}

Json::Value Uptane::Target::toJson() const {
  Json::Value res;
  res["custom"]["ecuIdentifier"] = ecu_identifier_;
  res["custom"]["targetFormat"] = type_;

  std::vector<Uptane::Hash>::const_iterator it;
  for (it = hashes_.begin(); it != hashes_.end(); it++) {
    res["hashes"][it->TypeString()] = it->HashString();
  }
  res["length"] = Json::Value((Json::Value::Int64)length_);
  return res;
}

bool Target::MatchWith(const Hash &hash) const {
  return (std::find(hashes_.begin(), hashes_.end(), hash) != hashes_.end());
}

std::string Target::sha256Hash() const {
  std::vector<Uptane::Hash>::const_iterator it;
  for (it = hashes_.begin(); it != hashes_.end(); it++) {
    if (it->type() == Hash::kSha256) {
      return boost::algorithm::to_lower_copy(it->HashString());
    }
  }
  return std::string();
}

std::ostream &Uptane::operator<<(std::ostream &os, const Target &t) {
  os << "Target(" << t.filename_ << " ecu_identifier:" << t.ecu_identifier() << " length:" << t.length();
  os << " hashes: (";
  for (std::vector<Hash>::const_iterator it = t.hashes_.begin(); it != t.hashes_.end(); ++it) {
    os << *it << ", ";
  }
  os << "))";

  return os;
}

Uptane::Targets::Targets(const Json::Value &json) {
  if (!json.isObject() || json["_type"] != "Targets")
    throw Uptane::InvalidMetadata("", "targets", "invalid targets.json");

  version = json["version"].asInt();
  expiry = Uptane::TimeStamp(json["expires"].asString());

  Json::Value target_list = json["targets"];
  for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    Target t(t_it.key().asString(), *t_it);
    targets.push_back(t);
  }
}

Uptane::Targets::Targets() { version = -1; }

Json::Value Uptane::Targets::toJson() const {
  Json::Value res;
  res["_type"] = "Targets";
  res["expires"] = expiry.ToString();

  std::vector<Uptane::Target>::const_iterator it;
  for (it = targets.begin(); it != targets.end(); it++) {
    res["targets"][it->filename()] = it->toJson();
  }
  res["version"] = version;
  return res;
}

Uptane::TimestampMeta::TimestampMeta(const Json::Value &json) {
  if (!json.isObject() || json["_type"] != "Timestamp")
    throw Uptane::InvalidMetadata("", "timestamp", "invalid timestamp.json");

  version = json["version"].asInt();
  expiry = Uptane::TimeStamp(json["expires"].asString());

  // TODO: METAFILES
}

Uptane::TimestampMeta::TimestampMeta() { version = -1; }

Json::Value Uptane::TimestampMeta::toJson() const {
  Json::Value res;
  res["_type"] = "Timestamp";
  res["expires"] = expiry.ToString();
  res["version"] = version;
  // TODO: METAFILES
  return res;
}

Uptane::Snapshot::Snapshot(const Json::Value &json) {
  if (!json.isObject() || json["_type"] != "Snapshot")
    throw Uptane::InvalidMetadata("", "snapshot", "invalid snapshot.json");

  version = json["version"].asInt();
  expiry = Uptane::TimeStamp(json["expires"].asString());

  Json::Value meta_list = json["meta"];
  for (Json::ValueIterator m_it = meta_list.begin(); m_it != meta_list.end(); m_it++)
    versions[m_it.key().asString()] = (*m_it)["version"].asInt();
}

Uptane::Snapshot::Snapshot() { version = -1; }

Json::Value Uptane::Snapshot::toJson() const {
  Json::Value res;
  res["_type"] = "Snapshot";
  res["expires"] = expiry.ToString();

  std::map<std::string, int>::const_iterator it;
  for (it = versions.begin(); it != versions.end(); it++) {
    res["meta"][it->first]["version"] = it->second;
  }
  res["version"] = version;
  return res;
}
