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

std::ostream &Uptane::operator<<(std::ostream &os, const Hash &h) {
  os << "Hash: " << h.hash_;
  return os;
}

Target::Target(const std::string &filename, const Json::Value &content) : filename_(filename), ecu_identifier_("") {
  if (content.isMember("custom")) {
    Json::Value custom = content["custom"];
    if (custom.isMember("ecuIdentifier")) {
      ecu_identifier_ = content["custom"]["ecuIdentifier"].asString();
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
}

bool Target::MatchWith(const Hash &hash) const {
  return (std::find(hashes_.begin(), hashes_.end(), hash) != hashes_.end());
}

std::ostream &Uptane::operator<<(std::ostream &os, const Target &t) {
  os << "Target(" << t.filename_ << " ecu_identifier:" << t.ecu_identifier() << " length:" << t.length();
  os << " hashes: (";
  for (std::vector<Hash>::const_iterator it = t.hashes_.begin(); it != t.hashes_.end(); ++it) {
    std::cout << *it << ", ";
  }
  os << "))";

  return os;
}
