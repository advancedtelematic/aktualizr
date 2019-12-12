#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <memory>
#include <string>
#include <utility>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "storage_config.h"
#include "storage_exception.h"

#include "uptane/tuf.h"
#include "utilities/types.h"

class INvStorage;
class FSStorageRead;
class SQLStorage;

using store_data_t = void (INvStorage::*)(const std::string&);
using load_data_t = bool (INvStorage::*)(std::string*);

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
  size_t getWrittenSize() { return written_size_; }

  friend std::istream& operator>>(std::istream& is, StorageTargetWHandle& handle) {
    std::array<uint8_t, 256> arr{};
    while (!is.eof()) {
      is.read(reinterpret_cast<char*>(arr.data()), arr.size());
      handle.wfeed(arr.data(), static_cast<size_t>(is.gcount()));
    }
    return is;
  }

 protected:
  size_t written_size_{0};
};

class StorageTargetRHandle {
 public:
  class ReadError : public std::runtime_error {
   public:
    explicit ReadError(const std::string& what) : std::runtime_error(what) {}
  };
  virtual ~StorageTargetRHandle() = default;
  virtual bool isPartial() const = 0;
  virtual std::unique_ptr<StorageTargetWHandle> toWriteHandle() = 0;

  virtual size_t rsize() const = 0;
  virtual size_t rread(uint8_t* buf, size_t size) = 0;
  virtual void rclose() = 0;

  void writeToFile(const boost::filesystem::path& path) {
    std::array<uint8_t, 1024> arr{};
    size_t written = 0;
    std::ofstream file(path.c_str());
    if (!file.good()) {
      throw std::runtime_error(std::string("Error opening file ") + path.string());
    }
    while (written < rsize()) {
      size_t nread = rread(arr.data(), arr.size());
      file.write(reinterpret_cast<char*>(arr.data()), static_cast<std::streamsize>(nread));
      written += nread;
    }
    file.close();
  }

  // FIXME this function loads the whole image to the memory
  friend std::ostream& operator<<(std::ostream& os, StorageTargetRHandle& handle) {
    std::array<uint8_t, 256> arr{};
    size_t written = 0;
    while (written < handle.rsize()) {
      size_t nread = handle.rread(arr.data(), arr.size());

      os.write(reinterpret_cast<char*>(arr.data()), static_cast<std::streamsize>(nread));
      written += nread;
    }

    return os;
  }
};

enum class InstalledVersionUpdateMode { kNone, kCurrent, kPending };

// Functions loading/storing multiple pieces of data are supposed to do so atomically as far as implementation makes it
// possible
class INvStorage {
 public:
  explicit INvStorage(StorageConfig config) : config_(std::move(config)) {}
  virtual ~INvStorage() = default;
  virtual StorageType type() = 0;
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

  virtual void storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) = 0;
  virtual bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) = 0;
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) {
    return loadRoot(data, repo, Uptane::Version());
  };
  virtual void storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) = 0;
  virtual bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) = 0;
  virtual void clearNonRootMeta(Uptane::RepositoryType repo) = 0;
  virtual void clearMetadata() = 0;
  virtual void storeDelegation(const std::string& data, Uptane::Role role) = 0;
  virtual bool loadDelegation(std::string* data, Uptane::Role role) = 0;
  virtual bool loadAllDelegations(std::vector<std::pair<Uptane::Role, std::string>>& data) const = 0;
  virtual void deleteDelegation(Uptane::Role role) = 0;
  virtual void clearDelegations() = 0;

  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) = 0;
  virtual void clearDeviceId() = 0;

  virtual void storeEcuSerials(const EcuSerials& serials) = 0;
  virtual bool loadEcuSerials(EcuSerials* serials) = 0;
  virtual void clearEcuSerials() = 0;

  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) = 0;
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) = 0;
  virtual void clearMisconfiguredEcus() = 0;

  virtual void storeEcuRegistered() = 0;  // should be called after storeDeviceId
  virtual bool loadEcuRegistered() = 0;
  virtual void clearEcuRegistered() = 0;

  virtual void storeNeedReboot() = 0;
  virtual bool loadNeedReboot(bool* need_reboot) = 0;
  virtual void clearNeedReboot() = 0;

  virtual void saveInstalledVersion(const std::string& ecu_serial, const Uptane::Target& target,
                                    InstalledVersionUpdateMode update_mode) = 0;
  virtual bool loadInstalledVersions(const std::string& ecu_serial, boost::optional<Uptane::Target>* current_version,
                                     boost::optional<Uptane::Target>* pending_version) = 0;
  virtual bool loadInstallationLog(const std::string& ecu_serial, std::vector<Uptane::Target>* log,
                                   bool only_installed) = 0;
  virtual bool hasPendingInstall() = 0;
  virtual void clearInstalledVersions() = 0;

  virtual void saveEcuInstallationResult(const Uptane::EcuSerial& ecu_serial,
                                         const data::InstallationResult& result) = 0;
  virtual bool loadEcuInstallationResults(
      std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) = 0;
  virtual void storeDeviceInstallationResult(const data::InstallationResult& result, const std::string& raw_report,
                                             const std::string& correlation_id) = 0;
  virtual bool loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                            std::string* correlation_id) = 0;
  virtual void clearInstallationResults() = 0;

  virtual void saveEcuReportCounter(const Uptane::EcuSerial& ecu_serial, int64_t counter) = 0;
  virtual bool loadEcuReportCounter(std::vector<std::pair<Uptane::EcuSerial, int64_t>>* results) = 0;

  virtual boost::optional<std::pair<size_t, std::string>> checkTargetFile(const Uptane::Target& target) const = 0;

  // Incremental file API
  virtual std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director,
                                                                   const Uptane::Target& target) = 0;

  virtual std::unique_ptr<StorageTargetRHandle> openTargetFile(const Uptane::Target& target) = 0;
  virtual std::vector<Uptane::Target> getTargetFiles() = 0;
  virtual void removeTargetFile(const std::string& target_name) = 0;

  virtual void cleanUp() = 0;

  // Special constructors and utilities
  static std::shared_ptr<INvStorage> newStorage(const StorageConfig& config, bool readonly = false);
  static void FSSToSQLS(FSStorageRead& fs_storage, SQLStorage& sql_storage);
  static bool fsReadInstalledVersions(const boost::filesystem::path& filename,
                                      std::vector<Uptane::Target>* installed_versions, size_t* current_version);

  // Not purely virtual
  void importData(const ImportConfig& import_config);
  bool loadPrimaryInstalledVersions(boost::optional<Uptane::Target>* current_version,
                                    boost::optional<Uptane::Target>* pending_version) {
    return loadInstalledVersions("", current_version, pending_version);
  }
  void savePrimaryInstalledVersion(const Uptane::Target& target, InstalledVersionUpdateMode update_mode) {
    return saveInstalledVersion("", target, update_mode);
  }
  bool loadPrimaryInstallationLog(std::vector<Uptane::Target>* log, bool only_installed) {
    return loadInstallationLog("", log, only_installed);
  }
  void importInstalledVersions(const boost::filesystem::path& base_path);

 private:
  void importSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                    const BasedPath& imported_data_path);
  void importUpdateSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                          const BasedPath& imported_data_path);
  void importPrimaryKeys(const boost::filesystem::path& base_path, const BasedPath& import_pubkey_path,
                         const BasedPath& import_privkey_path);

 protected:
  const StorageConfig config_;
};

#endif  // INVSTORAGE_H_
