#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>
#include "invstorage.h"

class FSStorage {
 public:
  explicit FSStorage(const StorageConfig& config, bool migration_only = false);
  ~FSStorage() = default;
  void storePrimaryKeys(const std::string& public_key, const std::string& private_key);
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  bool loadPrimaryPublic(std::string* public_key);
  bool loadPrimaryPrivate(std::string* private_key);
  void clearPrimaryKeys();

  void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  void storeTlsCa(const std::string& ca);
  void storeTlsCert(const std::string& cert);
  void storeTlsPkey(const std::string& pkey);
  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  void clearTlsCreds();
  bool loadTlsCa(std::string* ca);
  bool loadTlsCert(std::string* cert);
  bool loadTlsPkey(std::string* pkey);

  void storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version);
  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version);
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) {
    return loadRoot(data, repo, Uptane::Version());
  };
  void storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role);
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role);
  void clearNonRootMeta(Uptane::RepositoryType repo);
  void clearMetadata();

  void storeDeviceId(const std::string& device_id);
  bool loadDeviceId(std::string* device_id);
  void clearDeviceId();
  void storeEcuSerials(const EcuSerials& serials);
  bool loadEcuSerials(EcuSerials* serials);
  void clearEcuSerials();
  void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus);
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus);
  void clearMisconfiguredEcus();
  void storeEcuRegistered();
  bool loadEcuRegistered();
  void clearEcuRegistered();
  void storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                              const std::string& current_hash);
  std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions);
  void clearInstalledVersions();
  void storeInstallationResult(const data::OperationResult& result);
  bool loadInstallationResult(data::OperationResult* result);
  void clearInstallationResult();

  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size);
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename);
  void removeTargetFile(const std::string& filename);
  void cleanUp();
  StorageType type() { return StorageType::kFileSystem; };

  friend class FSTargetWHandle;
  friend class FSTargetRHandle;

 private:
  const StorageConfig& config_;

  // descriptors of currently downloaded files
  std::map<std::string, FILE*> director_files;
  std::map<std::string, FILE*> image_files;

  Uptane::Version latest_director_root;
  Uptane::Version latest_images_root;

  boost::filesystem::path targetFilepath(const std::string& filename) const;
  bool loadTlsCommon(std::string* data, const BasedPath& path_in);

  bool splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version);
  Uptane::Version findMaxVersion(const boost::filesystem::path& meta_directory, Uptane::Role role);
};

#endif  // FSSTORAGE_H_
