#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>
#include "invstorage.h"

class FSStorage {
 public:
  explicit FSStorage(const StorageConfig& config);
  ~FSStorage() = default;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  bool loadPrimaryPublic(std::string* public_key);
  bool loadPrimaryPrivate(std::string* private_key);
  void clearPrimaryKeys();

  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  void clearTlsCreds();
  bool loadTlsCa(std::string* ca);
  bool loadTlsCert(std::string* cert);
  bool loadTlsPkey(std::string* pkey);

  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version);
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) {
    return loadRoot(data, repo, Uptane::Version());
  };
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role);
  void clearNonRootMeta(Uptane::RepositoryType repo);
  void clearMetadata();

  bool loadDeviceId(std::string* device_id);
  void clearDeviceId();
  bool loadEcuSerials(EcuSerials* serials);
  void clearEcuSerials();
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus);
  void clearMisconfiguredEcus();
  bool loadEcuRegistered();
  void clearEcuRegistered();
  std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions);
  void clearInstalledVersions();
  bool loadInstallationResult(data::OperationResult* result);
  void clearInstallationResult();

  void cleanUp();

 private:
  const StorageConfig& config_;

  Uptane::Version latest_director_root;
  Uptane::Version latest_images_root;

  bool loadTlsCommon(std::string* data, const BasedPath& path_in);

  bool splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version);
  Uptane::Version findMaxVersion(const boost::filesystem::path& meta_directory, Uptane::Role role);
};

#endif  // FSSTORAGE_H_
