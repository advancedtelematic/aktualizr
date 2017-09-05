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
  virtual bool loadTlsCa(std::string* ca);  // TODO: may be deleted when new requirements on pkcs11 arrive
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

 private:
  Config config_;
  // descriptors of currently downloaded files
  std::map<std::string, FILE*> director_files;
  std::map<std::string, FILE*> image_files;
};

#endif  // FSSTORAGE_H_
