#include "get.h"
#include "crypto/keymanager.h"
#include "http/httpclient.h"
#include "storage/invstorage.h"

std::string aktualizrGet(Config &config, const std::string &url, const std::vector<std::string> &headers,
                         StorageClient storage_client) {
  auto storage = INvStorage::newStorage(config.storage, false, storage_client);
  storage->importData(config.import);

  auto client = std_::make_unique<HttpClient>(&headers);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.copyCertsToCurl(*client);
  auto resp = client->get(url, HttpInterface::kNoLimit);
  if (resp.http_status_code != 200) {
    throw std::runtime_error("Unable to get " + url + ": HTTP_" + std::to_string(resp.http_status_code) + "\n" +
                             resp.body);
  }
  return resp.body;
}
