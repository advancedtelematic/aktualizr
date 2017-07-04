#include "uptane/tufrepository.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>

#include <algorithm>
#include <fstream>

#include "crypto.h"
#include "logger.h"
#include "utils.h"

namespace Uptane {

TufRepository::TufRepository(const std::string& name, const std::string& base_url, const Config& config)
    : name_(name), path_(config.uptane.metadata_path / name_), config_(config), base_url_(base_url) {
  boost::filesystem::create_directories(path_);
  boost::filesystem::create_directories(path_ / "targets");
  http_.authenticate((config_.device.certificates_directory / config_.tls.client_certificate).string(),
                     (config_.device.certificates_directory / config_.tls.ca_file).string(),
                     (config_.device.certificates_directory / config_.tls.pkey_file).string());
  LOGGER_LOG(LVL_debug, "TufRepository looking for root.json in:" << path_);
  if (boost::filesystem::exists(path_ / "root.json")) {
    initRoot(Utils::parseJSONFile(path_ / "root.json"));
  }
  if (boost::filesystem::exists(path_ / "timestamp.json")) {
    timestamp_signed_ = Utils::parseJSONFile(path_ / "timestamp.json");
  } else {
    timestamp_signed_["version"] = 0;
  }
}

Json::Value TufRepository::getJSON(const std::string& role) {
  HttpResponse response = http_.get(base_url_ + "/" + role);
  if (!response.isOk()) {
    throw MissingRepo(name_);
  }
  return response.getJson();
}

void TufRepository::updateRoot() {
  Json::Value content = getJSON("root.json");
  initRoot(content);
  saveRole(content);
}

bool TufRepository::checkTimestamp() {
  Json::Value content = updateRole("timestamp.json");
  bool has_changed = (content["signed"]["version"].asUInt() > timestamp_signed_["version"].asUInt());
  timestamp_signed_ = content["signed"];
  return has_changed;
}

Json::Value TufRepository::updateRole(const std::string& role) {
  Json::Value content = getJSON(role);
  verifyRole(content);
  saveRole(content);
  return content;
}

bool TufRepository::hasExpired(const std::string& date) {
  if (date.size() != 20 || date[19] != 'Z') {
    throw Uptane::Exception(name_, "Wrong expires datetime field!!!");
  }
  try {
    boost::posix_time::ptime parsed_date(boost::gregorian::from_string(date.substr(0, 10)),
                                         boost::posix_time::duration_from_string(date.substr(11, 8)));
    return boost::posix_time::second_clock::local_time() > parsed_date;
  } catch (std::exception e) {
    throw Uptane::Exception(name_, std::string("Wrong expires datetime field, what(): ") + e.what());
  }
  return false;
}

void TufRepository::verifyRole(const Json::Value& tuf_signed) {
  std::string role = boost::algorithm::to_lower_copy(tuf_signed["signed"]["_type"].asString());
  if (!tuf_signed["signatures"].size()) {
    throw SecurityException(name_, "Missing signatures, verification failed");
  } else if (tuf_signed["signatures"].size() < thresholds_[role]) {
    throw UnmetThreshold(name_, role);
  }

  std::string canonical = Json::FastWriter().write(tuf_signed["signed"]);
  std::vector<std::string> signatures;
  for (Json::ValueIterator it = tuf_signed["signatures"].begin(); it != tuf_signed["signatures"].end(); it++) {
    std::string signature = (*it)["sig"].asString();
    if (std::find(signatures.begin(), signatures.end(), signature) != signatures.end()) {
      throw NonUniqueSignatures(name_, role);
    } else {
      signatures.push_back(signature);
    }
    std::string method((*it)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss" && method != "rsassa-pss-sha256" && method != "ed25519") {
      throw SecurityException(name_, std::string("Unsupported sign method: ") + (*it)["method"].asString());
    }
    std::string keyid = (*it)["keyid"].asString();
    if (!keys_.count(keyid)) {
      throw SecurityException(name_, std::string("Couldn't find a key: ") + keyid);
    }
    if (!Crypto::VerifySignature(keys_[keyid], signature, canonical)) {
      throw SecurityException(name_, "Invalid signature, verification failed");
    }
  }
  if (hasExpired(tuf_signed["signed"]["expires"].asString())) {
    throw ExpiredMetadata(name_, role);
  }
}

void TufRepository::saveRole(const Json::Value& content) {
  std::string role = boost::algorithm::to_lower_copy(content["signed"]["_type"].asString());
  std::ofstream file((path_ / (role + ".json")).string().c_str());
  file << content;
  file.close();
}

std::string TufRepository::downloadTarget(Target target) {
  HttpResponse response = http_.get(base_url_ + "/" + target.filename_);
  if (!response.isOk()) {
    throw Exception(name_, "Could not download file");
  }
  if (response.body.length() > target.length_) {
    throw OversizedTarget(name_);
  }
  if (!target.hash_.matchWith(response.body.c_str())) {
    throw TargetHashMismatch(name_, HASH_METADATA_MISMATCH);
  }
  return response.body;
}

void TufRepository::saveTarget(const Target& target) {
  if (target.length_ > 0) {
    std::string content = downloadTarget(target);
    Utils::writeFile((path_ / "targets" / target.filename_).string(), content);
  }
  targets_.push_back(target);
}

void TufRepository::updateKeys(const Json::Value& keys) {
  for (Json::ValueIterator it = keys.begin(); it != keys.end(); it++) {
    std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException(name_, "Unsupported key type: " + (*it)["keytype"].asString());
    }
    PublicKey pkey((*it)["keyval"]["public"].asString(), (*it)["keytype"].asString());
    if (pkey.type == PublicKey::RSA && pkey.key_length < 256) {
      throw IllegalRsaKeySize(name_);
    }
    keys_.insert(std::make_pair(it.key().asString(), pkey));
  }
}

bool TufRepository::findSignatureByKeyId(const Json::Value& signatures, const std::string& keyid) {
  for (Json::ValueIterator it = signatures.begin(); it != signatures.end(); it++) {
    if ((*it)["keyid"].asString() == keyid) {
      return true;
    }
  }
  return false;
}

void TufRepository::initRoot(const Json::Value& new_content) {
  keys_.clear();
  thresholds_.clear();
  Json::Value content(new_content);
  unsigned int version = content["signed"]["version"].asUInt();
  if (version > 1) {
    Json::Value prev_root;
    boost::filesystem::path prev_root_path = path_ / (Utils::intToString(version - 1) + ".root.json");
    if (boost::filesystem::exists(prev_root_path)) {
      prev_root = Utils::parseJSONFile(prev_root_path);
    } else {
      prev_root = getJSON(Utils::intToString(version - 1) + ".root.json");
    }

    Json::Value keyids = prev_root["signed"]["roles"]["root"]["keyids"];
    Json::Value prev_keys;
    for (Json::ValueIterator it = keyids.begin(); it != keyids.end(); it++) {
      if (!findSignatureByKeyId(content["signatures"], (*it).asString())) {
        throw UnmetThreshold(name_, "root");
      }
      prev_keys[(*it).asString()] = prev_root["signed"]["keys"][(*it).asString()];
    }
    updateKeys(prev_keys);
    thresholds_["root"] = prev_root["signed"]["roles"]["root"]["threshold"].asInt();
  }
  updateKeys(content["signed"]["keys"]);
  Json::Value json_roles = content["signed"]["roles"];
  for (Json::ValueIterator it = json_roles.begin(); it != json_roles.end(); it++) {
    std::string role = it.key().asString();
    int requiredThreshold = (*it)["threshold"].asInt();
    if (requiredThreshold < kMinSignatures) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
                                                               << kMinSignatures);
      throw IllegalThreshold(name_, "The role " + role + " had an illegal signature threshold.");
    }
    if (kMaxSignatures < requiredThreshold) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too large: " << kMaxSignatures << " < "
                                                               << requiredThreshold);
      throw IllegalThreshold(name_, "root.json contains a role that requires too many signatures");
    }
    if (!config_.uptane.disable_keyid_validation) {
      for (Json::ValueIterator it_keyid = (*it)["keyids"].begin(); it_keyid != (*it)["keyids"].end(); ++it_keyid) {
        if (keys_.count((*it_keyid).asString())) {
          std::string key_can = Json::FastWriter().write(Json::Value(keys_[(*it_keyid).asString()].value));
          std::string key_id = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(key_can)));
          if ((*it_keyid).asString() != key_id) {
            throw UnmetThreshold(name_, role);
          }
        }
      }
    }
    thresholds_[role] += requiredThreshold;
  }
  verifyRole(content);
}

void TufRepository::refresh() {
  targets_.clear();
  if (checkTimestamp()) {
    Json::Value content = updateRole("snapshot.json");
    Json::Value updated_roles = content["signed"]["meta"];
    std::vector<std::string> files_to_update;
    bool init_root = false;
    for (Json::ValueIterator it = updated_roles.begin(); it != updated_roles.end(); it++) {
      if (boost::ends_with(it.key().asString(), "root.json")) {
        init_root = true;
      } else {
        files_to_update.push_back(it.key().asString());
      }
    }
    if (init_root) {  // Root should be updated first
      initRoot(updateRole("root.json"));
    }
    for (std::vector<std::string>::iterator it = files_to_update.begin(); it != files_to_update.end(); it++) {
      Json::Value new_content = updateRole(*it);
      if ((*it) == "targets.json") {
        Json::Value json_targets = new_content["signed"]["targets"];
        for (Json::ValueIterator t_it = json_targets.begin(); t_it != json_targets.end(); t_it++) {
          Json::Value hashes = (*t_it)["hashes"];
          Hasher hash;
          if (hashes.isMember("sha512")) {
            hash = Hasher(Hasher::sha512, hashes["sha512"].asString());
          } else if (hashes.isMember("sha256")) {
            hash = Hasher(Hasher::sha256, hashes["sha256"].asString());
          }
          Target t(Target((*t_it)["custom"], t_it.key().asString(), (*t_it)["length"].asUInt64(), hash));
          saveTarget(t);
        }
      }
    }
  }
}
};