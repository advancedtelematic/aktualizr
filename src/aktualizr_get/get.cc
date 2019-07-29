#include "get.h"
#include "crypto/keymanager.h"
#include "http/httpclient.h"

std::string aktualizrGet(Config &config, const std::string url) {
  auto storage = INvStorage::newStorage(config.storage);
  storage->importData(config.import);

  auto client = std_::make_unique<HttpClient>();
  KeyManager keys(storage, config.keymanagerConfig());
  keys.copyCertsToCurl(*client);
  auto resp = client->get(url, HttpInterface::kNoLimit);
  if (resp.http_status_code != 200) {
    throw std::runtime_error("Unable to get " + url + ": HTTP_" + std::to_string(resp.http_status_code) + "\n" +
                             resp.body);
  }
  return resp.body;
}
