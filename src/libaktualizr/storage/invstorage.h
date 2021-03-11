#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "libaktualizr/config.h"
#include "storage_exception.h"
#include "uptane/tuf.h"

class INvStorage;
class FSStorageRead;
class SQLStorage;

enum class StorageClient { kUptane = 0, kTUF = 1 };

using store_data_t = void (INvStorage::*)(const std::string&);
using load_data_t = bool (INvStorage::*)(std::string*) const;

typedef std::vector<std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier>> EcuSerials;

// kUnused was previously kNotRegistered, but re-registration is now possible so
// that is no longer a misconfiguration.
enum class EcuState { kOld = 0, kUnused };

struct MisconfiguredEcu {
  MisconfiguredEcu(Uptane::EcuSerial serial_in, Uptane::HardwareIdentifier hardware_id_in, EcuState state_in)
      : serial(std::move(serial_in)), hardware_id(std::move(hardware_id_in)), state(state_in) {}
  Uptane::EcuSerial serial;
  Uptane::HardwareIdentifier hardware_id;
  EcuState state;
};

enum class InstalledVersionUpdateMode { kNone, kCurrent, kPending };

// Functions loading/storing multiple pieces of data are supposed to do so
// atomically as far as implementation makes it possible.
//
// store* functions normally write the complete content. save* functions just add an entry.
class INvStorage {
 public:
  explicit INvStorage(StorageConfig config, StorageClient client = StorageClient::kUptane)
      : config_(std::move(config)), client_(client) {}
  virtual ~INvStorage() = default;
  virtual StorageType type() = 0;
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key) = 0;
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key) const = 0;
  virtual bool loadPrimaryPublic(std::string* public_key) const = 0;
  virtual bool loadPrimaryPrivate(std::string* private_key) const = 0;
  virtual void clearPrimaryKeys() = 0;

  virtual void saveSecondaryInfo(const Uptane::EcuSerial& ecu_serial, const std::string& sec_type,
                                 const PublicKey& public_key) = 0;
  virtual void saveSecondaryData(const Uptane::EcuSerial& ecu_serial, const std::string& data) = 0;
  virtual bool loadSecondaryInfo(const Uptane::EcuSerial& ecu_serial, SecondaryInfo* secondary) const = 0;
  virtual bool loadSecondariesInfo(std::vector<SecondaryInfo>* secondaries) const = 0;

  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual void storeTlsCa(const std::string& ca) = 0;
  virtual void storeTlsCert(const std::string& cert) = 0;
  virtual void storeTlsPkey(const std::string& pkey) = 0;
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) const = 0;
  virtual bool loadTlsCa(std::string* ca) const = 0;
  virtual bool loadTlsCert(std::string* cert) const = 0;
  virtual bool loadTlsPkey(std::string* cert) const = 0;
  virtual void clearTlsCreds() = 0;

  virtual void storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) = 0;
  virtual bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) const = 0;
  bool loadLatestRoot(std::string* data, Uptane::RepositoryType repo) const {
    return loadRoot(data, repo, Uptane::Version());
  };
  virtual void storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) = 0;
  virtual bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) const = 0;
  virtual void clearNonRootMeta(Uptane::RepositoryType repo) = 0;
  virtual void clearMetadata() = 0;
  virtual void storeDelegation(const std::string& data, Uptane::Role role) = 0;
  virtual bool loadDelegation(std::string* data, Uptane::Role role) const = 0;
  virtual bool loadAllDelegations(std::vector<std::pair<Uptane::Role, std::string>>& data) const = 0;
  virtual void deleteDelegation(Uptane::Role role) = 0;
  virtual void clearDelegations() = 0;

  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) const = 0;
  virtual void clearDeviceId() = 0;

  virtual void storeEcuSerials(const EcuSerials& serials) = 0;
  virtual bool loadEcuSerials(EcuSerials* serials) const = 0;
  virtual void clearEcuSerials() = 0;

  virtual void storeCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, const std::string& manifest) = 0;
  virtual bool loadCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, std::string* manifest) const = 0;

  virtual void saveMisconfiguredEcu(const MisconfiguredEcu& ecu) = 0;
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) const = 0;
  virtual void clearMisconfiguredEcus() = 0;

  virtual void storeEcuRegistered() = 0;  // should be called after storeDeviceId
  virtual bool loadEcuRegistered() const = 0;
  virtual void clearEcuRegistered() = 0;

  virtual void storeNeedReboot() = 0;
  virtual bool loadNeedReboot(bool* need_reboot) const = 0;
  virtual void clearNeedReboot() = 0;

  virtual void saveInstalledVersion(const std::string& ecu_serial, const Uptane::Target& target,
                                    InstalledVersionUpdateMode update_mode) = 0;
  virtual bool loadInstalledVersions(const std::string& ecu_serial, boost::optional<Uptane::Target>* current_version,
                                     boost::optional<Uptane::Target>* pending_version) const = 0;
  virtual bool loadInstallationLog(const std::string& ecu_serial, std::vector<Uptane::Target>* log,
                                   bool only_installed) const = 0;
  virtual bool hasPendingInstall() = 0;
  virtual void getPendingEcus(std::vector<std::pair<Uptane::EcuSerial, Hash>>* pendingEcus) = 0;
  virtual void clearInstalledVersions() = 0;

  virtual void saveEcuInstallationResult(const Uptane::EcuSerial& ecu_serial,
                                         const data::InstallationResult& result) = 0;
  virtual bool loadEcuInstallationResults(
      std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) const = 0;
  virtual void storeDeviceInstallationResult(const data::InstallationResult& result, const std::string& raw_report,
                                             const std::string& correlation_id) = 0;
  virtual bool storeDeviceInstallationRawReport(const std::string& raw_report) = 0;
  virtual bool loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                            std::string* correlation_id) const = 0;
  virtual void clearInstallationResults() = 0;

  virtual void saveEcuReportCounter(const Uptane::EcuSerial& ecu_serial, int64_t counter) = 0;
  virtual bool loadEcuReportCounter(std::vector<std::pair<Uptane::EcuSerial, int64_t>>* results) const = 0;

  virtual void saveReportEvent(const Json::Value& json_value) = 0;
  virtual bool loadReportEvents(Json::Value* report_array, int64_t* id_max) const = 0;
  virtual void deleteReportEvents(int64_t id_max) = 0;

  virtual void storeDeviceDataHash(const std::string& data_type, const std::string& hash) = 0;
  virtual bool loadDeviceDataHash(const std::string& data_type, std::string* hash) const = 0;
  virtual void clearDeviceData() = 0;

  // Downloaded files info API
  virtual void storeTargetFilename(const std::string& targetname, const std::string& filename) const = 0;
  virtual std::string getTargetFilename(const std::string& targetname) const = 0;
  virtual std::vector<std::string> getAllTargetNames() const = 0;
  virtual void deleteTargetInfo(const std::string& targetname) const = 0;

  virtual void cleanUp() = 0;

  // Special constructors and utilities
  static std::shared_ptr<INvStorage> newStorage(const StorageConfig& config, bool readonly = false,
                                                StorageClient client = StorageClient::kUptane);
  static void FSSToSQLS(FSStorageRead& fs_storage, SQLStorage& sql_storage);
  static bool fsReadInstalledVersions(const boost::filesystem::path& filename,
                                      std::vector<Uptane::Target>* installed_versions, size_t* current_version);

  // Not purely virtual
  void importData(const ImportConfig& import_config);
  bool loadPrimaryInstalledVersions(boost::optional<Uptane::Target>* current_version,
                                    boost::optional<Uptane::Target>* pending_version) const {
    return loadInstalledVersions("", current_version, pending_version);
  }
  void savePrimaryInstalledVersion(const Uptane::Target& target, InstalledVersionUpdateMode update_mode) {
    return saveInstalledVersion("", target, update_mode);
  }
  bool loadPrimaryInstallationLog(std::vector<Uptane::Target>* log, bool only_installed) const {
    return loadInstallationLog("", log, only_installed);
  }
  void importInstalledVersions(const boost::filesystem::path& base_path);

 private:
  void importSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                    const utils::BasedPath& imported_data_path, const std::string& data_name);
  void importUpdateSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                          const utils::BasedPath& imported_data_path, const std::string& data_name);
  void importUpdateCertificate(const boost::filesystem::path& base_path, const utils::BasedPath& imported_data_path);
  void importPrimaryKeys(const boost::filesystem::path& base_path, const utils::BasedPath& import_pubkey_path,
                         const utils::BasedPath& import_privkey_path);

 protected:
  const StorageConfig config_;

 private:
  StorageClient client_;
};

#endif  // INVSTORAGE_H_
