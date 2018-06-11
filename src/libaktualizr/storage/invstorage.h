#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <boost/filesystem.hpp>
#include <memory>
#include <string>
#include <utility>

#include "uptane/tuf.h"

#include "storage_config.h"

class INvStorage;
class FSStorage;
class SQLStorage;

using store_data_t = void (INvStorage::*)(const std::string&);
using load_data_t = bool (INvStorage::*)(std::string*);

typedef std::pair<std::string, bool> InstalledVersion;

typedef std::vector<std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier>> EcuSerials;

enum class EcuState { kOld = 0, kNotRegistered };

struct MisconfiguredEcu {
  MisconfiguredEcu(Uptane::EcuSerial serial_in, Uptane::HardwareIdentifier hardware_id_in, EcuState state_in)
      : serial(std::move(serial_in)), hardware_id(std::move(hardware_id_in)), state(state_in) {}
  Uptane::EcuSerial serial;
  Uptane::HardwareIdentifier hardware_id;
  EcuState state;
};

class StorageTargetWHandle {
 public:
  class WriteError : public std::runtime_error {
   public:
    explicit WriteError(const std::string& what) : std::runtime_error(what) {}
  };
  virtual ~StorageTargetWHandle() = default;
  virtual size_t wfeed(const uint8_t* buf, size_t size) = 0;
  virtual void wcommit() = 0;
  virtual void wabort() = 0;

  friend std::istream& operator>>(std::istream& is, StorageTargetWHandle& handle) {
    std::array<uint8_t, 256> arr{};
    while (!is.eof()) {
      is.read(reinterpret_cast<char*>(arr.data()), arr.size());

      handle.wfeed(arr.data(), is.gcount());
    }
    handle.wcommit();

    return is;
  }
};

class StorageTargetRHandle {
 public:
  class ReadError : public std::runtime_error {
   public:
    explicit ReadError(const std::string& what) : std::runtime_error(what) {}
  };
  virtual ~StorageTargetRHandle() = default;
  virtual size_t rsize() const = 0;
  virtual size_t rread(uint8_t* buf, size_t size) = 0;
  virtual void rclose() = 0;

  friend std::ostream& operator<<(std::ostream& os, StorageTargetRHandle& handle) {
    std::array<uint8_t, 256> arr{};
    size_t written = 0;
    while (written < handle.rsize()) {
      size_t nread = handle.rread(arr.data(), arr.size());

      os.write(reinterpret_cast<char*>(arr.data()), nread);
      written += nread;
    }

    return os;
  }
};

// Functions loading/storing multiple pieces of data are supposed to do so atomically as far as implementation makes it
// possible
class INvStorage {
 public:
  explicit INvStorage(const StorageConfig& config) : config_(config) {}
  virtual ~INvStorage() = default;
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key) = 0;
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

  virtual void storeMetadata(const Uptane::RawMetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::RawMetaPack* metadata) = 0;
  virtual void clearMetadata() = 0;

  virtual void storeUncheckedMetadata(const Uptane::RawMetaPack& metadata) = 0;
  virtual bool loadUncheckedMetadata(Uptane::RawMetaPack* metadata) = 0;
  virtual void clearUncheckedMetadata() = 0;

  virtual void storeRoot(bool director, const std::string& root, Uptane::Version version) = 0;
  virtual bool loadRoot(bool director, std::string* root, Uptane::Version version) = 0;

  virtual void storeUncheckedRoot(bool director, const std::string& root, Uptane::Version version) = 0;
  virtual bool loadUncheckedRoot(bool director, std::string* root, Uptane::Version version) = 0;

  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) = 0;
  virtual void clearDeviceId() = 0;

  virtual void storeEcuSerials(const EcuSerials& serials) = 0;
  virtual bool loadEcuSerials(EcuSerials* serials) = 0;
  virtual void clearEcuSerials() = 0;

  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) = 0;
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) = 0;
  virtual void clearMisconfiguredEcus() = 0;

  virtual void storeEcuRegistered() = 0;
  virtual bool loadEcuRegistered() = 0;
  virtual void clearEcuRegistered() = 0;

  virtual void storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                                      const std::string& current_hash) = 0;
  virtual std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) = 0;
  virtual void clearInstalledVersions() = 0;

  // Incremental file API
  virtual std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                                   size_t size) = 0;
  virtual std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) = 0;
  virtual void removeTargetFile(const std::string& filename) = 0;

  virtual void cleanUp() = 0;

  // Not purely virtual
  virtual void importData(const ImportConfig& import_config);
  void saveInstalledVersion(const Uptane::Target& target);

  static std::shared_ptr<INvStorage> newStorage(const StorageConfig& config,
                                                const boost::filesystem::path& path = "/var/sota");
  static void FSSToSQLS(const std::shared_ptr<INvStorage>& fs_storage, std::shared_ptr<INvStorage>& sql_storage);
  virtual StorageType type() = 0;

 private:
  void importSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importUpdateSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importPrimaryKeys(const boost::filesystem::path& import_pubkey_path,
                         const boost::filesystem::path& import_privkey_path);

 protected:
  const StorageConfig& config_;
};

#endif  // INVSTORAGE_H_
