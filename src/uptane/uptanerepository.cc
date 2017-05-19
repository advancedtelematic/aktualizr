#include "uptane/uptanerepository.h"
#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "ostree.h"
#include "utils.h"

namespace Uptane {

Repository::Repository(const Config& config_in)
    : config(config_in),
      director("director", config.uptane.director_server, config),
      image("repo", config.uptane.repo_server, config) {
      }

void Repository::updateRoot(){
    director.updateRoot();
    image.updateRoot();
}


Json::Value Repository::sign(const Json::Value& in_data) {
  std::string key_path = (config.device.certificates_path / config.uptane.private_key_path).string();
  std::string b64sig = Utils::toBase64(Crypto::RSAPSSSign(key_path, Json::FastWriter().write(in_data)));

  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  std::ifstream key_path_stream(key_path.c_str());
  std::string key_content((std::istreambuf_iterator<char>(key_path_stream)), std::istreambuf_iterator<char>());
  std::string keyid = boost::algorithm::hex(Crypto::sha256digest(key_content));
  std::transform(keyid.begin(), keyid.end(), keyid.begin(), ::tolower);
  Json::Value out_data;
  signature["keyid"] = keyid;
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return out_data;
}

std::string Repository::signManifest() { return signManifest(Json::nullValue); }

std::string Repository::signManifest(const Json::Value& custom) {
  Json::Value version_manifest;
  version_manifest["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  version_manifest["ecu_version_manifest"] = Json::Value(Json::arrayValue);
  Json::Value ecu_version_signed = sign(OstreePackage::getEcu(config.uptane.primary_ecu_serial).toEcuVersion(custom));
  version_manifest["ecu_version_manifest"].append(ecu_version_signed);
  Json::Value tuf_signed = sign(version_manifest);
  return Json::FastWriter().write(tuf_signed);
}

std::vector<Uptane::Target> Repository::getNewTargets() {
  director.refresh();
  image.refresh();
  std::vector<Uptane::Target> targets = director.getTargets();
  //std::equal(targets.begin(), targets.end(), image.getTargets().begin());
  return targets;
}
};
