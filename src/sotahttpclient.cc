#include "sotahttpclient.h"

#include <json/json.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>
#include "time.h"

#include "crypto.h"
#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in, event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in), events_channel(events_channel_in), commands_channel(commands_channel_in) {
  http = new HttpClient();

  if (config.core.auth_type == CERTIFICATE) {
    if (!boost::filesystem::exists(config.device.certificates_path + config.tls.client_certificate) ||
        !boost::filesystem::exists(config.device.certificates_path + config.tls.ca_file)) {
      if (!deviceRegister() || !ecuRegister()) {
        throw std::runtime_error(
            "Fatal error of tls or ecu device registration, please look at previous error log messages to understand "
            "the reason");
      }
    }
    core_url = config.core.server + "/api/v1";
  } else {
    core_url = config.tls.server + "/api/v1";
  }

  processing = false;
  retries = SotaHttpClient::MAX_RETRIES;
  boost::thread(boost::bind(&SotaHttpClient::run, this));
}

SotaHttpClient::~SotaHttpClient() { delete http; }

SotaHttpClient::SotaHttpClient(const Config &config_in, HttpClient *http_in, event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in), http(http_in), events_channel(events_channel_in), commands_channel(commands_channel_in) {
  core_url = config.core.server + "/api/v1";
}

bool SotaHttpClient::deviceRegister() {
  std::string bootstrap_cert_pem = config.device.certificates_path + "bootstrap_cert.pem";
  std::string bootstrap_ca_pem = config.device.certificates_path + "bootstrap_ca.pem";

  FILE *reg_p12 = fopen((config.device.certificates_path + config.provision.p12_path).c_str(), "rb");
  if (!reg_p12) {
    LOGGER_LOG(LVL_error, "Could not open " << config.device.certificates_path + config.provision.p12_path);
    fclose(reg_p12);
    return false;
  }

  if (!Crypto::parseP12(reg_p12, config.provision.p12_password, bootstrap_cert_pem, bootstrap_ca_pem)) {
    fclose(reg_p12);
    return false;
  }
  fclose(reg_p12);

  http->setCerts(bootstrap_ca_pem, bootstrap_cert_pem);
  std::string data =
      "{\"deviceId\":\"" + config.provision.device_id + "\", \"ttl\":" + config.provision.expiry_days + "}";
  std::string result = http->post(config.tls.server + "/devices", data);
  if (http->http_code != 200 && http->http_code != 201) {
    LOGGER_LOG(LVL_error, "error tls registering device, response: " << result);
    return false;
  }

  FILE *device_p12 = fmemopen(const_cast<char *>(result.c_str()), result.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", config.device.certificates_path + config.tls.client_certificate,
                        config.device.certificates_path + config.tls.ca_file)) {
    return false;
  }
  fclose(device_p12);
  return true;
}

bool SotaHttpClient::ecuRegister() {
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
  std::string public_key = config.device.certificates_path + config.uptane.public_key_path;
  BIO *bp_public = BIO_new_file(public_key.c_str(), "w");
  ret = PEM_write_bio_RSAPublicKey(bp_public, r);
  if (ret != 1) {
    BN_free(bne);
    RSA_free(r);
    BIO_free_all(bp_public);
    return false;
  }

  BIO *bp_private = BIO_new_file((config.device.certificates_path + config.uptane.private_key_path).c_str(), "w");
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
  pub_key_str = boost::replace_all_copy(pub_key_str, "\\n", "\n");
  std::string data = "{\"primary_ecu_serial\":\"" + config.provision.device_id + "\", \"ecus\":[{\"ecu_serial\":\"" +
                     config.provision.device_id +
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

bool SotaHttpClient::authenticate() {
  if (config.core.auth_type == CERTIFICATE) {
    return http->authenticate(config.device.certificates_path + config.tls.client_certificate,
                              config.device.certificates_path + config.tls.ca_file);
  } else {
    return http->authenticate(config.auth);
  }
}

std::vector<data::UpdateRequest> SotaHttpClient::getAvailableUpdates() {
  processing = true;
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates";
  std::vector<data::UpdateRequest> update_requests;

  Json::Value json;
  try {
    json = http->getJson(url);
  } catch (std::runtime_error e) {
    retry();
    return update_requests;
  }
  unsigned int updates = json.size();
  for (unsigned int i = 0; i < updates; ++i) {
    update_requests.push_back(data::UpdateRequest::fromJson(json[i]));
  }
  if (!updates) {
    processing = false;
  }
  return update_requests;
}

void SotaHttpClient::startDownload(const data::UpdateRequestId &update_request_id) {
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates/" + update_request_id + "/download";
  std::string filename = config.device.packages_dir + update_request_id;

  if (!http->download(url, filename)) {
    retry();
    return;
  }

  data::DownloadComplete download_complete;
  download_complete.update_id = update_request_id;
  download_complete.signature = "";
  download_complete.update_image = filename;
  *events_channel << boost::make_shared<event::DownloadComplete>(download_complete);
}

void SotaHttpClient::sendUpdateReport(data::UpdateReport update_report) {
  std::string url = core_url + "/mydevice/" + config.device.uuid;
  url += "/updates/" + update_report.update_id;
  try {
    http->post(url, update_report.toJson()["operation_results"]);
  } catch (std::runtime_error e) {
    retry();
    return;
  }
  processing = true;
}

void SotaHttpClient::retry() {
  was_error = true;
  processing = false;
  retries--;
  if (!retries) {
    retries = SotaHttpClient::MAX_RETRIES;
    was_error = false;
  }
}

void SotaHttpClient::run() {
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
