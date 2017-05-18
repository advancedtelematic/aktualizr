#include "sotauptaneclient.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>

#include "crypto.h"
#include "json/json.h"
#include "logger.h"
#include "uptane/exceptions.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in), uptane_repo(config) {
  http = new HttpClient();
}

Json::Value SotaUptaneClient::getJSON(SotaUptaneClient::ServiceType service, const std::string &role) {
  return http->getJson(getEndPointUrl(service, role) + ".json");
}

std::string SotaUptaneClient::getEndPointUrl(SotaUptaneClient::ServiceType service, const std::string &role) {
  if (service == Director) {
    return config.uptane.director_server + "/" + role;
  } else {
    return config.uptane.repo_server + "/" + config.device.uuid + "/" + role;
  }
}

void SotaUptaneClient::putManifest(SotaUptaneClient::ServiceType service, const std::string &manifest) {
  http->put(getEndPointUrl(service, "manifest"), manifest);
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
      "{\"deviceId\":\"" + config.uptane.primary_ecu_serial + "\", \"ttl\":" + config.provision.expiry_days + "}";
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
  authenticate();
  return true;
}

bool SotaUptaneClient::ecuRegister() {
  int bits = 1024;
  int ret = 0;

  RSA *r = RSA_generate_key(bits,   /* number of bits for the key - 2048 is a sensible value */
                            RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
                            NULL,   /* callback - can be NULL if we aren't displaying progress */
                            NULL    /* callback argument - not needed in this case */
                            );
  EVP_PKEY *pkey = EVP_PKEY_new();
  EVP_PKEY_assign_RSA(pkey, r);
  std::string public_key = (config.device.certificates_path / config.uptane.public_key_path).string();
  BIO *bp_public = BIO_new_file(public_key.c_str(), "w");
  ret = PEM_write_bio_PUBKEY(bp_public, pkey);
  if (ret != 1) {
    RSA_free(r);
    BIO_free_all(bp_public);
    return false;
  }

  BIO *bp_private = BIO_new_file((config.device.certificates_path / config.uptane.private_key_path).c_str(), "w");
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
                     "\", \"ecus\":[{\"hardware_identifier\":\"" + config.device.uuid + "\",\"ecu_serial\":\"" +
                     config.uptane.primary_ecu_serial +
                     "\", \"clientKey\": {\"keytype\": \"RSA\", \"keyval\": {\"public\": \"" + pub_key_str + "\"}}}]}";

  std::string result = http->post(config.tls.server + "/director/ecus", data);
  if (http->http_code != 200 && http->http_code != 201) {
    LOGGER_LOG(LVL_error, "Error registering device on Uptane, response: " << result);
    return false;
  }
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
      *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    }
  }
}

std::vector<OstreePackage> SotaUptaneClient::getAvailableUpdates() {
  std::vector<OstreePackage> result;
  std::vector<Uptane::Target> targets = uptane_repo.getNewTargets();
  for (std::vector<Uptane::Target>::iterator it = targets.begin(); it != targets.end(); ++it) {
    result.push_back(OstreePackage((*it).custom_["ecuIdentifier"].asString(), (*it).filename_, (*it).hash_.hash_, "",
                                   (*it).custom_["uri"].asString()));
  }

  return result;
}

void SotaUptaneClient::OstreeInstall(std::vector<OstreePackage> packages) {
  processing = true;
  data::PackageManagerCredentials cred;
  cred.ca_file = (config.device.certificates_path / config.tls.ca_file).string();
  cred.pkey_file = (config.device.certificates_path / config.tls.pkey_file).string();
  cred.cert_file = (config.device.certificates_path / config.tls.client_certificate).string();
  for (std::vector<OstreePackage>::iterator it = packages.begin(); it != packages.end(); ++it) {
    data::InstallOutcome outcome = (*it).install(cred, config.ostree);
    data::OperationResult result = data::OperationResult::fromOutcome((*it).ref_name, outcome);
    Json::Value operation_result;
    operation_result["operation_result"] = result.toJson();
    putManifest(SotaUptaneClient::Director, uptane_repo.signManifest(operation_result));
  }
  processing = false;
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
  putManifest(Director, uptane_repo.signManifest());
  processing = false;
  boost::thread polling_thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");

    try {
      if (command->variant == "GetUpdateRequests") {
        std::vector<OstreePackage> updates = getAvailableUpdates();
        if (updates.size()) {
          LOGGER_LOG(LVL_info, "got new updates");
          *events_channel << boost::make_shared<event::UptaneTargetsUpdated>(updates);
        } else {
          LOGGER_LOG(LVL_info, "no new updates, sending UptaneTimestampUpdated event");
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
        }
      } else if (command->variant == "OstreeInstall") {
        command::OstreeInstall *ot_command = command->toChild<command::OstreeInstall>();
        std::vector<OstreePackage> packages = ot_command->packages;
        OstreeInstall(packages);
      } else if (command->variant == "Shutdown") {
        polling_thread.interrupt();
        return;
      }

    } catch (Uptane::SecurityException e) {
      LOGGER_LOG(LVL_error, e.what());
    }
  }
}
