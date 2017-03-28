#include "sotauptaneclient.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>
#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "json/json.h"
#include "logger.h"

std::string SotaUptaneClient::sign(const Json::Value &in_data, Json::Value &out_data) {
  std::string key_path = (config.device.certificates_path / config.uptane.private_key_path).string();
  std::string signature_str = Crypto::RSAPSSSign(key_path, Json::FastWriter().write(in_data));

  Json::Value signature;
  signature["method"] = "rsassa-pss";
  std::string b64sig(base64_text(signature_str.begin()), base64_text(signature_str.end()));
  b64sig.append((3 - signature_str.length() % 3) % 3, '=');
  signature["sig"] = b64sig;

  std::ifstream key_path_stream(key_path.c_str());
  std::string key_content((std::istreambuf_iterator<char>(key_path_stream)), std::istreambuf_iterator<char>());
  signature["keyid"] = boost::algorithm::hex(Crypto::sha256digest(key_content));
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return signature_str;
}

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in) {
  http = new HttpClient();
}

void SotaUptaneClient::initService(SotaUptaneClient::ServiceType service) {
  ecu_versions.push_back(OstreePackage::getEcu(config.uptane.primary_ecu_serial).toEcuVersion(""));
  Json::Value root_json = getJSON(service, "root", true);
  if (!root_json) {
    LOGGER_LOG(LVL_debug, "JSON is empty");
    return;
  }
  Json::Value json_keys = root_json["signed"]["keys"];
  for (Json::ValueIterator it = json_keys.begin(); it != json_keys.end(); it++) {
    services[service].keys[it.key().asString()] = (*it)["keyval"]["public"].asString();
  }

  Json::Value json_roles = root_json["signed"]["roles"];
  for (Json::ValueIterator it = json_roles.begin(); it != json_roles.end(); it++) {
    services[service].roles[it.key().asString()].first = (*it)["threshold"].asUInt();
  }

  if (!verifyData(service, "root", root_json)) {
    std::runtime_error("veryfication of root.json failed");
  }
}

Json::Value SotaUptaneClient::getJSON(SotaUptaneClient::ServiceType service, const std::string &role,
                                      bool force_fetch) {
  boost::filesystem::path path = config.uptane.metadata_path;
  if (service == Director) {
    path /= "director";
  } else {
    path /= "repo";
  }
  path /= role + ".json";

  if (!force_fetch || boost::filesystem::exists(path)) {
    std::ifstream path_stream(path.c_str());
    std::string json_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Value parsed_json;
    Json::Reader().parse(json_content, parsed_json);
    return parsed_json;
  } else {
    return http->getJson(getEndPointUrl(service, role));
  }
}

bool SotaUptaneClient::verify(SotaUptaneClient::ServiceType service, const std::string &role,
                              SotaUptaneClient::Verified &verified, bool force_fetch) {
  Json::Value data = getJSON(service, role, force_fetch);
  verified.old_version = services[service].roles[role].second;
  if (!verifyData(service, role, data)) {
    return false;
  }
  verified.new_version = services[service].roles[role].second;
  verified.role = role;
  verified.data = data["sigend"];
  return true;
}

bool SotaUptaneClient::verifyData(SotaUptaneClient::ServiceType service, const std::string &role,
                                  const Json::Value &tuf_signed) {
  if (!tuf_signed["signatures"].size()) {
    LOGGER_LOG(LVL_debug, "Missing signatures, verification failed");
    return false;
  }
  std::string canonical = Json::FastWriter().write(tuf_signed["signed"]);
  unsigned int valid_signatures = 0;
  for (Json::ValueIterator it = tuf_signed["signatures"].begin(); it != tuf_signed["signatures"].end(); it++) {
    if ((*it)["method"].asString() != "rsassa-pss") {
      LOGGER_LOG(LVL_debug, "Unknown sign method: " << (*it)["method"].asString());
      continue;
    }
    std::string keyid = (*it)["keyid"].asString();
    if (!services[service].keys.count(keyid)) {
      LOGGER_LOG(LVL_debug, "Couldn't find a key: " << keyid);
      continue;
    }
    std::string sigb64 = (*it)["sig"].asString();
    unsigned long long paddChars = std::count(sigb64.begin(), sigb64.end(), '=');
    std::replace(sigb64.begin(), sigb64.end(), '=', 'A');
    std::string sig(base64_to_bin(sigb64.begin()), base64_to_bin(sigb64.end()));
    sig.erase(sig.end() - static_cast<unsigned int>(paddChars), sig.end());

    // FIXME Hack, because of python signs interpreted new line but returns escaped
    // canonical = boost::replace_all_copy(canonical, "\\n", "\n");
    valid_signatures += Crypto::RSAPSSVerify(services[service].keys[keyid], sig, canonical);
  }
  if (valid_signatures == 0) {
    LOGGER_LOG(LVL_debug, "signature verification failed");
  } else if (valid_signatures < services[service].roles[role].first) {
    LOGGER_LOG(LVL_debug, "signature treshold error");
  } else {
    services[service].roles[role].second = tuf_signed["signed"]["version"].asUInt();
    return true;
  }
  return false;
}

std::string SotaUptaneClient::getEndPointUrl(SotaUptaneClient::ServiceType service, const std::string &role) {
  if (service == Director) {
    return config.uptane.director_server + "/" + role + ".json";
  } else {
    return config.uptane.repo_server + "/" + config.device.uuid + "/" + role + ".json";
  }
}

void SotaUptaneClient::putManfiest(SotaUptaneClient::ServiceType service) {
  Json::Value version_manifest;
  version_manifest["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  version_manifest["ecu_version_manifest"] = Json::Value(Json::arrayValue);
  for (std::vector<Json::Value>::iterator it = ecu_versions.begin(); it != ecu_versions.end(); ++it) {
    Json::Value ecu_version_signed;
    sign(*it, ecu_version_signed);
    version_manifest["ecu_version_manifest"].append(ecu_version_signed);
  }
  Json::Value tuf_signed;
  sign(version_manifest, tuf_signed);

  http->put(getEndPointUrl(service, "manifest"), Json::FastWriter().write(tuf_signed));
}

bool SotaUptaneClient::deviceRegister() {
  std::string bootstrap_pkey_pem = (config.device.certificates_path / "bootstrap_pkey.pem").string();
  std::string bootstrap_cert_pem = (config.device.certificates_path / "bootstrap_cert.pem").string();
  std::string bootstrap_ca_pem = (config.device.certificates_path / "bootstrap_ca.pem").string();

  FILE *reg_p12 = fopen((config.device.certificates_path / config.provision.p12_path).c_str(), "rb");
  if (!reg_p12) {
    LOGGER_LOG(LVL_error, "Could not open " << config.device.certificates_path / config.provision.p12_path);
    return false;
  }

  if (!Crypto::parseP12(reg_p12, config.provision.p12_password, bootstrap_pkey_pem, bootstrap_cert_pem,
                        bootstrap_ca_pem)) {
    fclose(reg_p12);
    return false;
  }
  fclose(reg_p12);

  http->setCerts(bootstrap_ca_pem, bootstrap_cert_pem, bootstrap_pkey_pem);
  std::string data =
      "{\"deviceId\":\"" + config.provision.device_id + "\", \"ttl\":" + config.provision.expiry_days + "}";
  std::string result = http->post(config.tls.server + "/devices", data);
  if (http->http_code != 200 && http->http_code != 201) {
    LOGGER_LOG(LVL_error, "error tls registering device, response: " << result);
    return false;
  }

  FILE *device_p12 = fmemopen(const_cast<char *>(result.c_str()), result.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", (config.device.certificates_path / config.tls.pkey_file).string(),
                        (config.device.certificates_path / config.tls.client_certificate).string(),
                        (config.device.certificates_path / config.tls.ca_file).string())) {
    return false;
  }
  fclose(device_p12);
  return true;
}

bool SotaUptaneClient::ecuRegister() {
  int bits = 1024;
  int ret = 0;
  RSA *r = RSA_new();

  BIGNUM *bne = BN_new();
  ret = BN_set_word(bne, RSA_F4);
  if (ret != 1) {
    BN_free(bne);
    return false;
  }

  RSA_generate_key_ex(r, bits, bne, NULL);

  EVP_PKEY *pkey = NULL;
  EVP_PKEY_assign_RSA(pkey, r);
  std::string public_key = (config.device.certificates_path / config.uptane.public_key_path).string();
  BIO *bp_public = BIO_new_file(public_key.c_str(), "w");
  ret = PEM_write_bio_RSAPublicKey(bp_public, r);
  if (ret != 1) {
    BN_free(bne);
    RSA_free(r);
    BIO_free_all(bp_public);
    return false;
  }

  BIO *bp_private = BIO_new_file((config.device.certificates_path / config.uptane.private_key_path).c_str(), "w");
  ret = PEM_write_bio_RSAPrivateKey(bp_private, r, NULL, NULL, 0, NULL, NULL);
  if (ret != 1) {
    BN_free(bne);
    RSA_free(r);
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    return false;
  }
  BN_free(bne);
  RSA_free(r);
  BIO_free_all(bp_public);
  BIO_free_all(bp_private);

  std::ifstream ks(public_key.c_str());
  std::string pub_key_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());
  ks.close();
  pub_key_str = boost::replace_all_copy(pub_key_str, "\n", "\\n");
  std::string data = "{\"primary_ecu_serial\":\"" + config.uptane.primary_ecu_serial +
                     "\", \"ecus\":[{\"ecu_serial\":\"" + config.uptane.primary_ecu_serial +
                     "\", \"clientKey\": {\"keytype\": \"RSA\", \"keyval\": {\"public\": \"" + pub_key_str + "\"}}}]}";

  std::string result = http->post(config.tls.server + "/director/ecus", data);
  if (http->http_code != 200 && http->http_code != 201) {
    LOGGER_LOG(LVL_error, "error uptone registering device, response: " << result);
    return false;
  }

  result = http->get(config.tls.server + "/director/root.json");
  if (http->http_code != 200) {
    LOGGER_LOG(LVL_error, "could not get director root metadata: " << result);
    return false;
  }

  boost::filesystem::create_directories(config.uptane.metadata_path / "director");

  std::ofstream director_file((config.uptane.metadata_path / "director/root.json").c_str());
  director_file << result;
  director_file.close();

  result = http->get(config.tls.server + "/repo/root.json");
  if (http->http_code != 200) {
    LOGGER_LOG(LVL_error, "could not get repo root metadata: " << result);
    return false;
  }
  boost::filesystem::create_directories(config.uptane.metadata_path / "repo");
  std::ofstream repo_file((config.uptane.metadata_path / "repo/root.json").c_str());
  repo_file << result;
  repo_file.close();

  return true;
}

bool SotaUptaneClient::authenticate() {
  return http->authenticate((config.device.certificates_path / config.tls.client_certificate).string(),
                            (config.device.certificates_path / config.tls.ca_file).string(),
                            (config.device.certificates_path / config.tls.pkey_file).string());
}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    if (processing) {
      sleep(1);
    } else {
      if (was_error) {
        sleep(2);
      } else {
        sleep(static_cast<unsigned int>(config.core.polling_sec));
      }
      if (!http->isAuthenticated()) {
        *commands_channel << boost::make_shared<command::Authenticate>();
      }
      *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    }
  }
}

std::vector<OstreePackage> SotaUptaneClient::getAvailableUpdates() {
  std::vector<OstreePackage> result;
  Verified verified;
  if (!verify(Director, "timestamp", verified, true)) {
    LOGGER_LOG(LVL_error, "bad signature of timestamp");
    return result;
  }
  if (verified.is_new()) {
    Verified verified_targets;
    if (!verify(Director, "targets", verified_targets, true)) {
      LOGGER_LOG(LVL_error, "bad signature of targets");
      return result;
    }
    Json::Value targets = verified_targets.data["signed"]["targets"];
    for (Json::Value::iterator it = targets.begin(); it != targets.end(); ++it) {
      Json::Value m_json = *it;
      result.push_back(OstreePackage(m_json["custom"]["ecuIdentifier"].asString(), it.key().asString(),
                                     m_json["hashes"]["sha256"].asString(), "", m_json["custom"]["uri"].asString()));
    }
  }
  return result;
}

void SotaUptaneClient::OstreeInstall(std::vector<OstreePackage> packages) {
  data::PackageManagerCredentials cred;
  cred.ca_file = (config.device.certificates_path / config.tls.ca_file).string();
  cred.pkey_file = (config.device.certificates_path / config.tls.pkey_file).string();
  cred.cert_file = (config.device.certificates_path / config.tls.client_certificate).string();
  for (std::vector<OstreePackage>::iterator it = packages.begin(); it != packages.end(); ++it) {
    if ((*it).install(cred).first != data::OK) {
      LOGGER_LOG(LVL_error, "error of installing package");
      return;
    }
  }
  putManfiest(SotaUptaneClient::Director);
}

void SotaUptaneClient::runForever(command::Channel *commands_channel) {
  if (!boost::filesystem::exists(config.device.certificates_path / config.tls.client_certificate) ||
      !boost::filesystem::exists(config.device.certificates_path / config.tls.ca_file)) {
    if (!deviceRegister() || !ecuRegister()) {
      throw std::runtime_error(
          "Fatal error of tls or ecu device registration, please look at previous error log messages to understand "
          "the reason");
    }
  }
  authenticate();

  initService(Director);

  putManfiest(Director);

  boost::thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");
    if (command->variant == "Authenticate") {
      if (authenticate()) {
        *events_channel << boost::make_shared<event::Authenticated>();
      } else {
        *events_channel << boost::make_shared<event::NotAuthenticated>();
      }
    } else if (command->variant == "GetUpdateRequests") {
      std::vector<OstreePackage> updates = getAvailableUpdates();
      if (updates.size()) {
        LOGGER_LOG(LVL_info, "got new updates");
        *commands_channel << boost::shared_ptr<command::OstreeInstall>(new command::OstreeInstall(updates));
      } else {
        LOGGER_LOG(LVL_info, "no new updates, sending NoUpdateRequests event");
        *events_channel << boost::shared_ptr<event::NoUpdateRequests>(new event::NoUpdateRequests());
      }
    } else if (command->variant == "OstreeInstall") {
      command::OstreeInstall *ot_command = command->toChild<command::OstreeInstall>();
      std::vector<OstreePackage> packages = ot_command->packages;
      OstreeInstall(packages);
    }
  }
}