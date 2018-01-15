#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <boost/filesystem.hpp>
#include <string>
#include "config.h"

#include "uptane/tuf.h"

class INvStorage;
class FSStorage;
class SQLStorage;

typedef void (INvStorage::*store_data_t)(const std::string& data);
typedef bool (INvStorage::*load_data_t)(std::string* data);

// Functions loading/storing multiple pieces of data are supposed to do so atomically as far as implementation makes it
// possible
class INvStorage {
 public:
  virtual ~INvStorage() {}
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key) = 0;
  virtual void storePrimaryPublic(const std::string& public_key) = 0;
  virtual void storePrimaryPrivate(const std::string& private_key) = 0;
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key) = 0;
  virtual bool loadPrimaryPublic(std::string* public_key) = 0;
  virtual bool loadPrimaryPrivate(std::string* private_key) = 0;
  virtual void clearPrimaryKeys() = 0;

  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual void storeTlsCa(const std::string& ca) = 0;
  virtual void storeTlsCert(const std::string& cert) = 0;
  virtual void storeTlsPkey(const std::string& pkey) = 0;
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) = 0;
  virtual bool loadTlsCa(std::string* ca) = 0;
  virtual bool loadTlsCert(std::string* cert) = 0;
  virtual bool loadTlsPkey(std::string* cert) = 0;
  virtual void clearTlsCreds() = 0;
#ifdef BUILD_OSTREE

  virtual void storeMetadata(const Uptane::MetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::MetaPack* metadata) = 0;
  virtual void clearMetadata() = 0;

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

  virtual void storeInstalledVersions(const std::map<std::string, std::string>& installed_versions) = 0;
  virtual bool loadInstalledVersions(std::map<std::string, std::string>* installed_versions) = 0;
  virtual void clearInstalledVersions() = 0;

  virtual void cleanUp() = 0;

  // Not purely virtual
  virtual void importData(const ImportConfig& import_config);

  // Incremental file API
  // virtual bool filePreallocate(bool from_director, const std::string &filename, size_t size) = 0;
  // virtual bool fileFeed(bool from_director, const std::string &filename, const uint8_t* buf, size_t len) = 0;
  // virtual bool fileCommit(bool from_director, const std::string &filename) = 0;
  // virtual void fileAbort(bool from_director, const std::string &filename) = 0;
  static boost::shared_ptr<INvStorage> newStorage(const StorageConfig& config,
                                                  const boost::filesystem::path& path = "/var/sota");
  static void FSSToSQLS(const boost::shared_ptr<INvStorage>& fs_storage, boost::shared_ptr<INvStorage>& sql_storage);

 private:
  void importSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importUpdateSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
};

#endif  // INVSTORAGE_H_
