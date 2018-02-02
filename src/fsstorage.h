#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>
#include "config.h"
#include "invstorage.h"

class FSStorage : public INvStorage {
 public:
  FSStorage(const StorageConfig& config);
  virtual ~FSStorage();
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key);
  virtual void storePrimaryPublic(const std::string& public_key);
  virtual void storePrimaryPrivate(const std::string& private_key);
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  virtual bool loadPrimaryPublic(std::string* public_key);
  virtual bool loadPrimaryPrivate(std::string* private_key);
  virtual void clearPrimaryKeys();

  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual void storeTlsCa(const std::string& ca);
  virtual void storeTlsCert(const std::string& cert);
  virtual void storeTlsPkey(const std::string& pkey);
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void clearTlsCreds();
  virtual bool loadTlsCa(std::string* ca);
  virtual bool loadTlsCert(std::string* cert);
  virtual bool loadTlsPkey(std::string* cert);
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
  virtual void clearMetadata();

  virtual void storeDeviceId(const std::string& device_id);
  virtual bool loadDeviceId(std::string* device_id);
  virtual void clearDeviceId();
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials);
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials);
  virtual void clearEcuSerials();
  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus);
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus);
  virtual void clearMisconfiguredEcus();
  virtual void storeEcuRegistered();
  virtual bool loadEcuRegistered();
  virtual void clearEcuRegistered();
  virtual void storeInstalledVersions(const std::map<std::string, std::string>& installed_versions);
  virtual bool loadInstalledVersions(std::map<std::string, std::string>* installed_versions);
  virtual void clearInstalledVersions();
  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) override;
  void removeTargetFile(const std::string& filename) override;
  virtual void cleanUp();
  virtual StorageType type() { return kFileSystem; };

  friend class FSTargetWHandle;
  friend class FSTargetRHandle;

 private:
  const StorageConfig& config_;

  boost::filesystem::path targetFilepath(const std::string& filename) const;
  bool loadTlsCommon(std::string* data, const boost::filesystem::path& path_in);
};

#endif  // FSSTORAGE_H_
