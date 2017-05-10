#include "uptane/tufrepository.h"

#include <boost/algorithm/string.hpp>
#include <fstream>

#include "crypto.h"
#include "utils.h"
namespace Uptane {

TufRepository::TufRepository(const boost::filesystem::path& path_in) : path(path_in) {
  boost::filesystem::create_directories(path);
  std::cout << "in this folder should be root: " << path.string() << "\n";
  if (boost::filesystem::exists(path / "root.json")) {
    std::ifstream path_stream((path / "root.json").string().c_str());
    std::string root_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Reader reader;
    Json::Value json_root;
    reader.parse(root_content, json_root);
    initRoot(json_root);
  }

  if (boost::filesystem::exists(path / "timestamp.json")) {
    std::ifstream path_stream((path / "timestamp.json").string().c_str());
    std::string json_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Reader reader;
    reader.parse(json_content, timestamp_signed);
  } else {
    timestamp_signed["version"] = 0;
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
  bool is_new = (content["signed"]["version"] > timestamp_signed["version"]);
  timestamp_signed = content["signed"];
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
  } else if (tuf_signed["signatures"].size() < tresholds[role]) {
    throw SecurityException("Signatures count is smaller then treshold, verification failed");
  }

  std::string canonical = Json::FastWriter().write(tuf_signed["signed"]);
  for (Json::ValueIterator it = tuf_signed["signatures"].begin(); it != tuf_signed["signatures"].end(); it++) {
    std::string method((*it)["method"].asString());
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method != "rsassa-pss") {
      throw SecurityException(std::string("Unsupported sign method: ") + (*it)["method"].asString());
    }
    std::string keyid = (*it)["keyid"].asString();
    if (!keys.count(keyid)) {
      throw SecurityException(std::string("Couldn't find a key: ") + keyid);
    }
    std::string sig = Utils::fromBase64((*it)["sig"].asString());

    if (!Crypto::RSAPSSVerify(keys[keyid].value, sig, canonical)) {
      throw SecurityException("Invalid signature, verification failed");
    }
  }
}

void TufRepository::saveRole(const Json::Value& content) {
  std::string role = boost::algorithm::to_lower_copy(content["signed"]["_type"].asString());
  std::ofstream file((path / (role + ".json")).string().c_str());
  file << content;
  file.close();
}

void TufRepository::initRoot(const Json::Value& content) {
  Json::Value json_keys = content["signed"]["keys"];
  for (Json::ValueIterator it = json_keys.begin(); it != json_keys.end(); it++) {
    if (boost::algorithm::to_lower_copy((*it)["keytype"].asString()) != "rsa") {
      throw SecurityException("Unsupported key type: " + (*it)["keytype"].asString());
    }
    keys.insert(std::make_pair(it.key().asString(),
                               PublicKey((*it)["keyval"]["public"].asString(), (*it)["keytype"].asString())));
  }

  Json::Value json_roles = content["signed"]["roles"];
  for (Json::ValueIterator it = json_roles.begin(); it != json_roles.end(); it++) {
    tresholds[it.key().asString()] = (*it)["treshold"].asUInt();
    ;
  }
}
};