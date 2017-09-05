#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <string>
#include "uptane/tuf.h"

class INvStorage {
 public:
  virtual ~INvStorage() { ; }
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key) = 0;
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key) = 0;
  virtual void clearPrimaryKeys() = 0;
  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) = 0;
  virtual void clearTlsCreds() = 0;
  virtual bool loadTlsCa(std::string* ca) = 0;  // TODO: may be deleted when new requirements on pkcs11 arrive
#ifdef BUILD_OSTREE
  virtual void storeMetadata(const Uptane::MetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::MetaPack* metadata) = 0;
#endif  // BUILD_OSTREE
  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) = 0;
  virtual void clearDeviceId() = 0;
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) = 0;
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) = 0;
  virtual void clearEcuSerials() = 0;
  virtual void storeEcuRegistered() = 0;
  virtual bool loadEcuRegistered() = 0;
  virtual void clearEcuRegistered() = 0;
  // Incremental file API
  // virtual bool filePreallocate(bool from_director, const std::string &filename, size_t size) = 0;
  // virtual bool fileFeed(bool from_director, const std::string &filename, const uint8_t* buf, size_t len) = 0;
  // virtual bool fileCommit(bool from_director, const std::string &filename) = 0;
  // virtual void fileAbort(bool from_director, const std::string &filename) = 0;
};

#endif  // INVSTORAGE_H_
