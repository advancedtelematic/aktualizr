#include "uptane/tufrepository.h"

#include <boost/algorithm/string.hpp>

#include <fstream>

#include "crypto.h"
#include "logger.h"
#include "utils.h"

namespace Uptane {

TufRepository::TufRepository(const boost::filesystem::path& path_in) : path_(path_in) {
  boost::filesystem::create_directories(path_);
  LOGGER_LOG(LVL_debug, "TufRepository looking for root.json in:" << path_);
  if (boost::filesystem::exists(path_ / "root.json")) {
    std::ifstream path_stream((path_ / "root.json").string().c_str());
    std::string root_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Reader reader;
    Json::Value json_root;
    reader.parse(root_content, json_root);
    initRoot(json_root);
  }

  if (boost::filesystem::exists(path_ / "timestamp.json")) {
    std::ifstream path_stream((path_ / "timestamp.json").string().c_str());
    std::string json_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Reader reader;
    reader.parse(json_content, timestamp_signed_);
  } else {
    timestamp_signed_["version"] = 0;
  }
}

void TufRepository::updateRoot(const Json::Value& content) {
  initRoot(content);
  verifyRole(content);
  saveRole(content);
}

bool TufRepository::updateTimestamp(const Json::Value& content) {
  verifyRole(content);
  saveRole(content);
  bool is_new = (content["signed"]["version"] > timestamp_signed_["version"]);
  timestamp_signed_ = content["signed"];
  return is_new;
}

void TufRepository::updateSnapshot(const Json::Value& content) {
  verifyRole(content);
  saveRole(content);
}

void TufRepository::updateTargets(const Json::Value& content) {
  verifyRole(content);
  saveRole(content);
}

void TufRepository::verifyRole(const Json::Value& tuf_signed) {
  std::string role = boost::algorithm::to_lower_copy(tuf_signed["signed"]["_type"].asString());
  if (!tuf_signed["signatures"].size()) {
    throw SecurityException("Missing signatures, verification failed");
  } else if (tuf_signed["signatures"].size() < thresholds_[role]) {
    throw SecurityException("Signatures count is smaller then threshold, verification failed");
  }

  std::string canonical = Json::FastWriter().write(tuf_signed["signed"]);
  for (Json::ValueIterator it = tuf_signed["signatures"].begin(); it != tuf_signed["signatures"].end(); it++) {
    std::string method((*it)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss" && method != "ed25519") {
      throw SecurityException(std::string("Unsupported sign method: ") + (*it)["method"].asString());
    }
    std::string keyid = (*it)["keyid"].asString();
    if (!keys_.count(keyid)) {
      throw SecurityException(std::string("Couldn't find a key: ") + keyid);
    }
    if (!Crypto::VerifySignature(keys_[keyid], (*it)["sig"].asString(), canonical)) {
      throw SecurityException("Invalid signature, verification failed");
    }
  }
}

void TufRepository::saveRole(const Json::Value& content) {
  std::string role = boost::algorithm::to_lower_copy(content["signed"]["_type"].asString());
  std::ofstream file((path_ / (role + ".json")).string().c_str());
  file << content;
  file.close();
}

void TufRepository::initRoot(const Json::Value& content) {
  Json::Value json_keys = content["signed"]["keys"];
  for (Json::ValueIterator it = json_keys.begin(); it != json_keys.end(); it++) {
    std::string key_type = boost::algorithm::to_lower_copy((*it)["keytype"].asString());
    if (key_type != "rsa" && key_type != "ed25519") {
      throw SecurityException("Unsupported key type: " + (*it)["keytype"].asString());
    }
    keys_.insert(std::make_pair(it.key().asString(),
                                PublicKey((*it)["keyval"]["public"].asString(), (*it)["keytype"].asString())));
  }

  Json::Value json_roles = content["signed"]["roles"];
  for (Json::ValueIterator it = json_roles.begin(); it != json_roles.end(); it++) {
    std::string role = it.key().asString();
    int requiredThreshold = (*it)["threshold"].asInt();
    if (requiredThreshold < kMinSignatures) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too small: " << requiredThreshold << " < "
                                                               << kMinSignatures);
      throw SecurityException("root.json contains a role that requires too few signatures");
    }
    if (kMaxSignatures < requiredThreshold) {
      LOGGER_LOG(LVL_debug, "Failing with threshold for role " << role << " too large: " << kMaxSignatures << " < "
                                                               << requiredThreshold);
      throw SecurityException("root.json contains a role that requires too many signatures");
    }
    thresholds_[role] = requiredThreshold;
  }
}
};