#include "uptane/uptanerepository.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "logger.h"
#include "ostree.h"
#include "utils.h"

namespace Uptane {

Repository::Repository(const Config &config_in)
    : config(config_in),
      director("director", config.uptane.director_server, config),
      image("repo", config.uptane.repo_server, config),
      http() {}

void Repository::updateRoot() {
  director.updateRoot();
  image.updateRoot();
}

Json::Value Repository::sign(const Json::Value &in_data) {
  std::string key_path = (config.device.certificates_directory / config.uptane.private_key_path).string();
  std::string b64sig = Utils::toBase64(Crypto::RSAPSSSign(key_path, Json::FastWriter().write(in_data)));

  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  std::string public_key_path = (config.device.certificates_directory / config.uptane.public_key_path).string();
  std::ifstream public_key_path_stream(public_key_path.c_str());
  std::string key_content((std::istreambuf_iterator<char>(public_key_path_stream)), std::istreambuf_iterator<char>());
  boost::algorithm::trim_right_if(key_content, boost::algorithm::is_any_of("\n"));
  std::string keyid = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(Json::Value(key_content))));
  std::transform(keyid.begin(), keyid.end(), keyid.begin(), ::tolower);
  Json::Value out_data;
  signature["keyid"] = keyid;
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return out_data;
}

void Repository::putManifest() { putManifest(Json::nullValue); }

void Repository::putManifest(const Json::Value &custom) {
  Json::Value version_manifest;
  version_manifest["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  version_manifest["ecu_version_manifest"] = Json::Value(Json::arrayValue);
  Json::Value ecu_version_signed =
      sign(OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).toEcuVersion(custom));
  version_manifest["ecu_version_manifest"].append(ecu_version_signed);
  Json::Value tuf_signed = sign(version_manifest);
  http.put(config.uptane.director_server + "/manifest", Json::FastWriter().write(tuf_signed));
}

std::vector<Uptane::Target> Repository::getNewTargets() {
  director.refresh();
  image.refresh();
  std::vector<Uptane::Target> targets = director.getTargets();
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
  int tries = 4;
  std::string result;
  do {
    std::string data =
        "{\"deviceId\":\"" + Utils::genPrettyName() + "\", \"ttl\":" + config.provision.expiry_days + "}";
    result = http.post(config.tls.server + "/devices", data);
    if (http.http_code != 200 && http.http_code != 201) {
      LOGGER_LOG(LVL_error, "error tls registering device, response: " << result);
      Json::Value result_json = Utils::parseJSON(result);
      if (result_json.isMember("code") && result_json["code"] == "device_already_registered") {
        continue;  // generate new name and try again
      }
      return false;
    }
  } while (--tries);

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
  int bits = 1024;
  int ret = 0;

  RSA *r = RSA_generate_key(bits,   /* number of bits for the key - 2048 is a sensible value */
                            RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
                            NULL,   /* callback - can be NULL if we aren't displaying progress */
                            NULL    /* callback argument - not needed in this case */
                            );
  EVP_PKEY *pkey = EVP_PKEY_new();
  EVP_PKEY_assign_RSA(pkey, r);
  std::string public_key = (config.device.certificates_directory / config.uptane.public_key_path).string();
  BIO *bp_public = BIO_new_file(public_key.c_str(), "w");
  ret = PEM_write_bio_PUBKEY(bp_public, pkey);
  if (ret != 1) {
    RSA_free(r);
    BIO_free_all(bp_public);
    return false;
  }

  BIO *bp_private = BIO_new_file((config.device.certificates_directory / config.uptane.private_key_path).c_str(), "w");
  ret = PEM_write_bio_RSAPrivateKey(bp_private, r, NULL, NULL, 0, NULL, NULL);
  if (ret != 1) {
    RSA_free(r);
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    return false;
  }
  RSA_free(r);
  BIO_free_all(bp_public);
  BIO_free_all(bp_private);

  std::ifstream ks(public_key.c_str());
  std::string pub_key_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());
  pub_key_str = pub_key_str.substr(0, pub_key_str.size() - 1);
  ks.close();
  pub_key_str = boost::replace_all_copy(pub_key_str, "\n", "\\n");

  std::string data = "{\"primary_ecu_serial\":\"" + config.uptane.primary_ecu_serial +
                     "\", \"ecus\":[{\"hardware_identifier\":\"" + config.uptane.primary_ecu_hardware_id +
                     "\",\"ecu_serial\":\"" + config.uptane.primary_ecu_serial +
                     "\", \"clientKey\": {\"keytype\": \"RSA\", \"keyval\": {\"public\": \"" + pub_key_str + "\"}}}]}";

  authenticate();
  std::string result = http.post(config.tls.server + "/director/ecus", data);
  if (http.http_code != 200 && http.http_code != 201) {
    LOGGER_LOG(LVL_error, "Error registering device on Uptane, response: " << result);
    return false;
  }
  return true;
}
};
