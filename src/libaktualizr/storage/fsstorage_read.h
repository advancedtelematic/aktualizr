#ifndef FSSTORAGE_READ_H_
#define FSSTORAGE_READ_H_

#include <boost/filesystem.hpp>
#include "invstorage.h"

class FSStorageRead {
 public:
  explicit FSStorageRead(const StorageConfig& config);
  ~FSStorageRead() = default;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  bool loadPrimaryPublic(std::string* public_key);
  bool loadPrimaryPrivate(std::string* private_key);

  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  bool loadTlsCa(std::string* ca);
  bool loadTlsCert(std::string* cert);
  bool loadTlsPkey(std::string* pkey);

  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version);
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) {
    return loadRoot(data, repo, Uptane::Version());
  };
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, const Uptane::Role& role);

  bool loadDeviceId(std::string* device_id);
  bool loadEcuSerials(EcuSerials* serials);
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus);
  bool loadEcuRegistered();
  bool loadInstalledVersions(std::vector<Uptane::Target>* installed_versions, size_t* current_version);

  void cleanUpAll();

  static bool FSStoragePresent(const StorageConfig& config);

 private:
  const StorageConfig& config_;

  Uptane::Version latest_director_root;
  Uptane::Version latest_image_root;

  bool loadTlsCommon(std::string* data, const BasedPath& path_in);

  bool splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version);
  Uptane::Version findMaxVersion(const boost::filesystem::path& meta_directory, const Uptane::Role& role);

  void clearPrimaryKeys();
  void clearTlsCreds();
  void clearNonRootMeta(Uptane::RepositoryType repo);
  void clearMetadata();
  void clearDeviceId();
  void clearEcuSerials();
  void clearMisconfiguredEcus();
  void clearEcuRegistered();
  void clearInstalledVersions();
  void clearInstallationResult();
};

#endif  // FSSTORAGE_READ_H_
