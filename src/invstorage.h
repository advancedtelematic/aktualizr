#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <boost/filesystem.hpp>
#include <memory>
#include <string>
#include "config.h"

#include "uptane/tuf.h"

class INvStorage;
class FSStorage;
class SQLStorage;

typedef void (INvStorage::*store_data_t)(const std::string& data);
typedef bool (INvStorage::*load_data_t)(std::string* data);

enum EcuState { kOld = 0, kNotRegistered };
struct MisconfiguredEcu {
  MisconfiguredEcu(const std::string& serial_in, const std::string hardware_id_in, EcuState state_in)
      : serial(serial_in), hardware_id(hardware_id_in), state(state_in) {}
  std::string serial;
  std::string hardware_id;
  EcuState state;
};

// Functions loading/storing multiple pieces of data are supposed to do so atomically as far as implementation makes it
// possible
class INvStorage {
 public:
   class TargetFileHandle {
     public:
       class AllocateError : public std::runtime_error {
         public:
           AllocateError(const std::string &what): std::runtime_error(what) {}
       };
       virtual size_t feed(const uint8_t *buf, size_t size) = 0;
       virtual void commit() = 0;
       virtual void abort() = 0;
   };

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

  virtual void storeMetadata(const Uptane::MetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::MetaPack* metadata) = 0;
  virtual void clearMetadata() = 0;

  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) = 0;
  virtual void clearDeviceId() = 0;

  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) = 0;
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) = 0;
  virtual void clearEcuSerials() = 0;

  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) = 0;
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) = 0;
  virtual void clearMisconfiguredEcus() = 0;

  virtual void storeEcuRegistered() = 0;
  virtual bool loadEcuRegistered() = 0;
  virtual void clearEcuRegistered() = 0;

  virtual void storeInstalledVersions(const std::map<std::string, std::string>& installed_versions) = 0;
  virtual bool loadInstalledVersions(std::map<std::string, std::string>* installed_versions) = 0;
  virtual void clearInstalledVersions() = 0;

  // Incremental file API
  virtual std::unique_ptr<TargetFileHandle> allocateFile(bool from_director,
      const std::string &filename, size_t size) = 0;

  virtual void cleanUp() = 0;

  // Not purely virtual
  virtual void importData(const ImportConfig& import_config);

  static boost::shared_ptr<INvStorage> newStorage(const StorageConfig& config,
                                                  const boost::filesystem::path& path = "/var/sota");
  static void FSSToSQLS(const boost::shared_ptr<INvStorage>& fs_storage, boost::shared_ptr<INvStorage>& sql_storage);
  virtual StorageType type() = 0;

 private:
  void importSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importUpdateSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
};

#endif  // INVSTORAGE_H_
