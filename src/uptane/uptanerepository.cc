#include "uptane/uptanerepository.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/make_shared.hpp>

#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "logger.h"
#include "openssl_compat.h"
#include "ostree.h"
#include "utils.h"

namespace Uptane {

Repository::Repository(const Config &config_in)
    : config(config_in),
      director("director", config.uptane.director_server, config),
      image("repo", config.uptane.repo_server, config),
      http(),
      manifests(Json::arrayValue),
      transport(&secondaries) {
  std::vector<Uptane::SecondaryConfig>::iterator it;
  for (it = config.uptane.secondaries.begin(); it != config.uptane.secondaries.end(); ++it) {
    Secondary s(*it, this);
    addSecondary(it->ecu_serial, it->ecu_hardware_id);
    secondaries.push_back(s);
  }
}

void Repository::updateRoot() {
  director.updateRoot();
  image.updateRoot();
}

void Repository::putManifest() { putManifest(Json::nullValue); }

void Repository::putManifest(const Json::Value &custom) {
  Json::Value version_manifest;
  version_manifest["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  version_manifest["ecu_version_manifest"] = transport.getManifests();

  Json::Value unsigned_ecu_version =
      OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).toEcuVersion(custom);
  Json::Value ecu_version_signed =
      Crypto::signTuf(config.uptane.private_key_path, config.uptane.public_key_path, unsigned_ecu_version);
  version_manifest["ecu_version_manifest"].append(ecu_version_signed);
  Json::Value tuf_signed =
      Crypto::signTuf(config.uptane.private_key_path, config.uptane.public_key_path, version_manifest);
  http.put(config.uptane.director_server + "/manifest", Json::FastWriter().write(tuf_signed));
}

std::vector<Uptane::Target> Repository::getNewTargets() {
  director.refresh();
  image.refresh();
  std::vector<Uptane::Target> targets = director.getTargets();
  if (!targets.empty()) {
    transport.sendTargets(targets);
  }
  // std::equal(targets.begin(), targets.end(), image.getTargets().begin());
  return targets;
}

bool Repository::deviceRegister() {
  std::string bootstrap_pkey_pem = (config.device.certificates_directory / "bootstrap_pkey.pem").string();
  std::string bootstrap_cert_pem = (config.device.certificates_directory / "bootstrap_cert.pem").string();
  std::string bootstrap_ca_pem = (config.device.certificates_directory / "bootstrap_ca.pem").string();

  FILE *reg_p12 = fopen((config.device.certificates_directory / config.provision.p12_path).c_str(), "rb");
  if (!reg_p12) {
    LOGGER_LOG(LVL_error, "Could not open " << config.device.certificates_directory / config.provision.p12_path);
    return false;
  }

  if (!Crypto::parseP12(reg_p12, config.provision.p12_password, bootstrap_pkey_pem, bootstrap_cert_pem,
                        bootstrap_ca_pem)) {
    fclose(reg_p12);
    return false;
  }
  fclose(reg_p12);

  http.setCerts(bootstrap_ca_pem, bootstrap_cert_pem, bootstrap_pkey_pem);
  std::string result;

  std::string data = "{\"deviceId\":\"" + config.uptane.device_id + "\", \"ttl\":" + config.provision.expiry_days + "}";
  result = http.post(config.tls.server + "/devices", data);
  if (http.http_code != 200 && http.http_code != 201) {
    LOGGER_LOG(LVL_error, "error tls registering device, response: " << result);
    return false;
  }

  FILE *device_p12 = fmemopen(const_cast<char *>(result.c_str()), result.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", (config.device.certificates_directory / config.tls.pkey_file).string(),
                        (config.device.certificates_directory / config.tls.client_certificate).string(),
                        (config.device.certificates_directory / config.tls.ca_file).string())) {
    return false;
  }
  fclose(device_p12);
  return true;
}

bool Repository::authenticate() {
  return http.authenticate((config.device.certificates_directory / config.tls.client_certificate).string(),
                           (config.device.certificates_directory / config.tls.ca_file).string(),
                           (config.device.certificates_directory / config.tls.pkey_file).string());
}

bool Repository::ecuRegister() {
  if (!Crypto::generateRSAKeyPair(config.uptane.public_key_path, config.uptane.private_key_path)) {
    LOGGER_LOG(LVL_error, "Could not generate rsa keys for primary.");
    return false;
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  all_ecus["ecus"] = Json::Value(Json::arrayValue);

  PublicKey primary_pub_key(config.uptane.public_key_path);
  Json::Value primary_ecu;
  primary_ecu["hardware_identifier"] = config.uptane.primary_ecu_hardware_id;
  primary_ecu["ecu_serial"] = config.uptane.primary_ecu_serial;
  primary_ecu["clientKey"]["keytype"] = "RSA";
  primary_ecu["clientKey"]["keyval"]["public"] = primary_pub_key.value;
  all_ecus["ecus"].append(primary_ecu);
  std::vector<SecondaryConfig>::iterator it;
  for (it = registered_secondaries.begin(); it != registered_secondaries.end(); ++it) {
    std::string pub_path = (config.device.certificates_directory / (it->ecu_serial+".pub")).string();
    std::string priv_path = (config.device.certificates_directory / (it->ecu_serial+".priv")).string();
    Crypto::generateRSAKeyPair(pub_path,
                               priv_path);
    transport.sendPrivateKey(it->ecu_serial, Utils::readFile(priv_path));
    Json::Value ecu;
    ecu["hardware_identifier"] = it->ecu_hardware_id;
    ecu["ecu_serial"] = it->ecu_serial;
    ecu["clientKey"]["keytype"] = "RSA";
    ecu["clientKey"]["keyval"]["public"] = Utils::readFile(pub_path);
    all_ecus["ecus"].append(ecu);

  }

  std::string data = Json::FastWriter().write(all_ecus);

  authenticate();
  std::string result = http.post(config.tls.server + "/director/ecus", data);
  if (http.http_code != 200 && http.http_code != 201) {
    LOGGER_LOG(LVL_error, "Error registering device on Uptane, response: " << result);
    return false;
  }
  return true;
}

void Repository::addSecondary(const std::string &ecu_serial, const std::string &hardware_identifier) {
  SecondaryConfig c;
  c.ecu_serial = ecu_serial;
  c.ecu_hardware_id = hardware_identifier;
  registered_secondaries.push_back(c);
}
};
