#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include "config.h"
#include "invstorage.h"

class FSStorage : public INvStorage {
 public:
  FSStorage(const Config& config);
  virtual ~FSStorage();
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key);
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  virtual void clearPrimaryKeys();
  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void clearTlsCreds();
  virtual bool loadTlsCa(std::string* ca);
  virtual bool loadTlsCert(std::string* cert);
  virtual bool loadTlsPkey(std::string* cert);
#ifdef BUILD_OSTREE
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
#endif  // BUILD_OSTREE
  virtual void storeDeviceId(const std::string& device_id);
  virtual bool loadDeviceId(std::string* device_id);
  virtual void clearDeviceId();
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials);
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials);
  virtual void clearEcuSerials();
  virtual void storeEcuRegistered();
  virtual bool loadEcuRegistered();
  virtual void clearEcuRegistered();
  virtual void storeInstalledVersions(const std::string& content);
  virtual bool loadInstalledVersions(std::string* content);

 private:
  Config config_;
  // descriptors of currently downloaded files
  std::map<std::string, FILE*> director_files;
  std::map<std::string, FILE*> image_files;

  bool loadTlsCommon(std::string* data, const std::string& path_in);
};

#endif  // FSSTORAGE_H_
