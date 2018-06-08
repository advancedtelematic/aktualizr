#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>
#include "invstorage.h"

class FSStorage : public INvStorage {
 public:
  explicit FSStorage(const StorageConfig& config, bool migration_only = false);
  ~FSStorage() override = default;
  void storePrimaryKeys(const std::string& public_key, const std::string& private_key) override;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key) override;
  bool loadPrimaryPublic(std::string* public_key) override;
  bool loadPrimaryPrivate(std::string* private_key) override;
  void clearPrimaryKeys() override;

  void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) override;
  void storeTlsCa(const std::string& ca) override;
  void storeTlsCert(const std::string& cert) override;
  void storeTlsPkey(const std::string& pkey) override;
  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) override;
  void clearTlsCreds() override;
  bool loadTlsCa(std::string* ca) override;
  bool loadTlsCert(std::string* cert) override;
  bool loadTlsPkey(std::string* pkey) override;
  void storeMetadata(const Uptane::RawMetaPack& metadata) override;
  bool loadMetadata(Uptane::RawMetaPack* metadata) override;
  void clearMetadata() override;
  void storeUncheckedMetadata(const Uptane::RawMetaPack& metadata) override;
  bool loadUncheckedMetadata(Uptane::RawMetaPack* metadata) override;
  void clearUncheckedMetadata() override;
  void storeRoot(bool director, const std::string& root, Uptane::Version version) override;
  bool loadRoot(bool director, std::string* root, Uptane::Version version) override;
  void storeUncheckedRoot(bool director, const std::string& root, Uptane::Version version) override;
  bool loadUncheckedRoot(bool director, std::string* root, Uptane::Version version) override;
  void storeDeviceId(const std::string& device_id) override;
  bool loadDeviceId(std::string* device_id) override;
  void clearDeviceId() override;
  void storeEcuSerials(const EcuSerials& serials) override;
  bool loadEcuSerials(EcuSerials* serials) override;
  void clearEcuSerials() override;
  void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) override;
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) override;
  void clearMisconfiguredEcus() override;
  void storeEcuRegistered() override;
  bool loadEcuRegistered() override;
  void clearEcuRegistered() override;
  void storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                              const std::string& current_hash) override;
  std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) override;
  void clearInstalledVersions() override;
  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) override;
  void removeTargetFile(const std::string& filename) override;
  void cleanUp() override;
  StorageType type() override { return StorageType::FileSystem; };

  friend class FSTargetWHandle;
  friend class FSTargetRHandle;

 private:
  // descriptors of currently downloaded files
  std::map<std::string, FILE*> director_files;
  std::map<std::string, FILE*> image_files;

  boost::filesystem::path targetFilepath(const std::string& filename) const;
  bool loadTlsCommon(std::string* data, const boost::filesystem::path& path_in);

  void storeMetadataCommon(const Uptane::RawMetaPack& metadata, const std::string& suffix);
  bool loadMetadataCommon(Uptane::RawMetaPack* metadata, const std::string& suffix);
  void clearMetadataCommon(const std::string& suffix);
  void storeRootCommon(bool director, const std::string& root, Uptane::Version version, const std::string& suffix);
  bool loadRootCommon(bool director, std::string* root, Uptane::Version version, const std::string& suffix);
};

#endif  // FSSTORAGE_H_
