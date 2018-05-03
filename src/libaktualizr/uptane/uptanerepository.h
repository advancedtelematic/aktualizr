#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <vector>

#include "json/json.h"

#include "config/config.h"
#include "crypto/crypto.h"
#include "crypto/keymanager.h"
#include "http/httpinterface.h"
#include "logging/logging.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"

namespace Uptane {

struct DownloadMetaStruct {
  int64_t expected_length{};
  int64_t downloaded_length{};
  StorageTargetWHandle *fhandle{};
  Hash::Type hash_type{Hash::kUnknownAlgorithm};
  MultiPartHasher &hasher() {
    switch (hash_type) {
      case Hash::kSha256:
        return sha256_hasher;
      case Hash::kSha512:
        return sha512_hasher;
      default:
        throw std::runtime_error("Unknown hash algorithm");
    }
  }
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

// TODO: must go
class Repository {
 public:
  Repository(const Config &config_in, std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);
  bool putManifest(const Json::Value &version_manifests);                         // TODO -> director
  Json::Value signVersionManifest(const Json::Value &primary_version_manifests);  // TODO -> sotauptaneclient
  void addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec) { secondary_info.push_back(sec); }
  std::pair<int, std::vector<Uptane::Target> > getTargets();               // TODO -> director
  std::string getPrimaryEcuSerial() const { return primary_ecu_serial; };  // TODO-> sotauptaneclient
  void setPrimaryEcuSerialHwId(const std::pair<std::string, std::string> &serials) {
    primary_ecu_serial = serials.first;
    primary_hardware_id = serials.second;
  }  // TODO-> a hack to be there until Uptane::Repository and SotaUptaneClient are merged

  bool getMeta();  // TODO -> split between director and image

  // TODO: only used by tests, rewrite test and delete this method
  void updateRoot(Version version = Version());
  // TODO: Receive and update time nonces.

  Uptane::MetaPack &currentMeta() { return meta_; }  // TODO -> split between director and image
  bool verifyMetaTargets(const Uptane::Targets &director_targets,
                         const Uptane::Targets &image_targets);  // TODO -> sotauptaneclient

 private:
  const Config &config;
  std::shared_ptr<INvStorage> storage;
  HttpInterface &http;

  KeyManager keys_;

  Json::Value manifests;
  MetaPack meta_;

  std::string primary_ecu_serial;
  std::string primary_hardware_id;

  std::vector<std::shared_ptr<Uptane::SecondaryInterface> > secondary_info;

  void downloadTarget(const Target &target);    // TODO -> sotauptaneclient
  Json::Value getJSON(const std::string &url);  // TODO -> metadownloader
  Json::Value fetchRole(const std::string &base_url, Uptane::Role role,
                        Version version = Version());  // TODO -> metadownloader
};
}  // namespace Uptane

#endif
