#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>

#include "config.h"
#include "invstorage.h"

class FSStorage : public INvStorage {
 public:
  FSStorage(const Config& config);
  virtual ~FSStorage();
  virtual void storeEcu(bool is_primary, const std::string& hardware_id, const std::string& ecu_id,
                        const std::string& public_key, const std::string& private_key);
  virtual bool loadEcuKeys(bool is_primary, const std::string& hardware_id, const std::string& ecu_id,
                           std::string* public_key, std::string* private_key);
  virtual void storeBootstrapTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual bool loadBootstrapTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
  // virtual bool filePreallocate(bool from_director, const std::string &filename, size_t size);
  // virtual bool fileFeed(bool from_director, const std::string &filename, const uint8_t* buf, size_t len);
  // virtual bool fileCommit(bool from_director, const std::string &filename);
  // virtual void fileAbort(bool from_director, const std::string &filename);

 private:
  Config config_;
  // descriptors of currently downloaded files
  std::map<std::string, FILE*> director_files;
  std::map<std::string, FILE*> image_files;

  bool loadTlsCredsCommon(const boost::filesystem::path& ca_path, const boost::filesystem::path& cert_path,
                          const boost::filesystem::path& pkey_path, std::string* ca, std::string* cert,
                          std::string* pkey);
  void storeTlsCredsCommon(const boost::filesystem::path& ca_path, const boost::filesystem::path& cert_path,
                           const boost::filesystem::path& pkey_path, const std::string& ca, const std::string& cert,
                           const std::string& pkey);
};

#endif  // FSSTORAGE_H_
