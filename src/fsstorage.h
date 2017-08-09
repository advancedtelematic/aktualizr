#ifndef FSSTORAGE_H_
#define FSSTORAGE_H_

#include <boost/filesystem.hpp>

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
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
  virtual void storeDeviceId(const std::string& device_id);
  virtual bool loadDeviceId(std::string* device_id);
  virtual void clearDeviceId();
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials);
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials);
  virtual void clearEcuSerials();
  virtual void storeEcuRegistered();
  virtual bool loadEcuRegistered();
  virtual void clearEcuRegistered();
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
