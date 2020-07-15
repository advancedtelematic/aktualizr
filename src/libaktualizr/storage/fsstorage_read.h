#ifndef FSSTORAGE_READ_H_
#define FSSTORAGE_READ_H_

#include <boost/filesystem.hpp>
#include "invstorage.h"

class FSStorageRead {
 public:
  explicit FSStorageRead(const StorageConfig& config);
  ~FSStorageRead() = default;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key) const;
  bool loadPrimaryPublic(std::string* public_key) const;
  bool loadPrimaryPrivate(std::string* private_key) const;

  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) const;
  bool loadTlsCa(std::string* ca) const;
  bool loadTlsCert(std::string* cert) const;
  bool loadTlsPkey(std::string* pkey) const;

  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) const;
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) const {
    return loadRoot(data, repo, Uptane::Version());
  };
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, const Uptane::Role& role) const;

  bool loadDeviceId(std::string* device_id) const;
  bool loadEcuSerials(EcuSerials* serials) const;
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) const;
  bool loadEcuRegistered() const;
  bool loadInstalledVersions(std::vector<Uptane::Target>* installed_versions, size_t* current_version) const;

  void cleanUpAll();

  static bool FSStoragePresent(const StorageConfig& config);

 private:
  const StorageConfig& config_;

  Uptane::Version latest_director_root;
  Uptane::Version latest_image_root;

  bool loadTlsCommon(std::string* data, const utils::BasedPath& path_in) const;

  static bool splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version);
  static Uptane::Version findMaxVersion(const boost::filesystem::path& meta_directory, const Uptane::Role& role);

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
