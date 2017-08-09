#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <string>
#include "uptane/tuf.h"

class INvStorage {
 public:
  virtual ~INvStorage() { ; }
  virtual void storeEcu(bool is_primary, const std::string& hardware_id, const std::string& ecu_id,
                        const std::string& public_key, const std::string& private_key) = 0;
  virtual bool loadEcuKeys(bool is_primary, const std::string& hardware_id, const std::string& ecu_id,
                           std::string* public_key, std::string* private_key) = 0;
  virtual void storeBootstrapTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual bool loadBootstrapTlsCreds(std::string* ca, std::string* cert, std::string* pkey) = 0;
  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) = 0;
  virtual void storeMetadata(const Uptane::MetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::MetaPack* metadata) = 0;
  // Incremental file API
  // virtual bool filePreallocate(bool from_director, const std::string &filename, size_t size) = 0;
  // virtual bool fileFeed(bool from_director, const std::string &filename, const uint8_t* buf, size_t len) = 0;
  // virtual bool fileCommit(bool from_director, const std::string &filename) = 0;
  // virtual void fileAbort(bool from_director, const std::string &filename) = 0;
};

#endif  // INVSTORAGE_H_
