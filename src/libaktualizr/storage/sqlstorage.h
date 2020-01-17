#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <sqlite3.h>

#include "invstorage.h"
#include "sqlstorage_base.h"

extern const std::vector<std::string> libaktualizr_schema_migrations;
extern const std::vector<std::string> libaktualizr_schema_rollback_migrations;
extern const std::string libaktualizr_current_schema;
extern const int libaktualizr_current_schema_version;

class SQLTargetRHandle;
class SQLStorage : public SQLStorageBase, public INvStorage {
 public:
  friend class SQLTargetWHandle;
  friend class SQLTargetRHandle;
  explicit SQLStorage(const StorageConfig& config, bool readonly);
  ~SQLStorage() override = default;
  void storePrimaryKeys(const std::string& public_key, const std::string& private_key) override;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key) override;
  bool loadPrimaryPublic(std::string* public_key) override;
  bool loadPrimaryPrivate(std::string* private_key) override;
  void clearPrimaryKeys() override;

  void saveSecondaryInfo(const Uptane::EcuSerial& ecu_serial, const std::string& sec_type,
                         const PublicKey& public_key) override;
  void saveSecondaryData(const Uptane::EcuSerial& ecu_serial, const std::string& data) override;
  bool loadSecondaryInfo(const Uptane::EcuSerial& ecu_serial, SecondaryInfo* secondary) override;
  bool loadSecondariesInfo(std::vector<SecondaryInfo>* secondaries) override;

  void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) override;
  void storeTlsCa(const std::string& ca) override;
  void storeTlsCert(const std::string& cert) override;
  void storeTlsPkey(const std::string& pkey) override;
  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) override;
  void clearTlsCreds() override;
  bool loadTlsCa(std::string* ca) override;
  bool loadTlsCert(std::string* cert) override;
  bool loadTlsPkey(std::string* pkey) override;

  void storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) override;
  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) override;
  void storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) override;
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) override;
  void clearNonRootMeta(Uptane::RepositoryType repo) override;
  void clearMetadata() override;
  void storeDelegation(const std::string& data, Uptane::Role role) override;
  bool loadDelegation(std::string* data, Uptane::Role role) override;
  bool loadAllDelegations(std::vector<std::pair<Uptane::Role, std::string>>& data) const override;
  void deleteDelegation(Uptane::Role role) override;
  void clearDelegations() override;

  void storeDeviceId(const std::string& device_id) override;
  bool loadDeviceId(std::string* device_id) override;
  void clearDeviceId() override;
  void storeEcuSerials(const EcuSerials& serials) override;
  bool loadEcuSerials(EcuSerials* serials) override;
  void clearEcuSerials() override;
  void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) override;
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) override;
  void clearMisconfiguredEcus() override;
  void storeEcuRegistered() override;
  bool loadEcuRegistered() override;
  void clearEcuRegistered() override;
  void storeNeedReboot() override;
  bool loadNeedReboot(bool* need_reboot) override;
  void clearNeedReboot() override;
  void saveInstalledVersion(const std::string& ecu_serial, const Uptane::Target& target,
                            InstalledVersionUpdateMode update_mode) override;
  bool loadInstalledVersions(const std::string& ecu_serial, boost::optional<Uptane::Target>* current_version,
                             boost::optional<Uptane::Target>* pending_version) override;
  bool loadInstallationLog(const std::string& ecu_serial, std::vector<Uptane::Target>* log,
                           bool only_installed) override;
  bool hasPendingInstall() override;
  void getPendingEcus(std::vector<std::pair<Uptane::EcuSerial, Uptane::Hash>>* pendingEcus) override;
  void clearInstalledVersions() override;

  void saveEcuInstallationResult(const Uptane::EcuSerial& ecu_serial, const data::InstallationResult& result) override;
  bool loadEcuInstallationResults(
      std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) override;
  void storeDeviceInstallationResult(const data::InstallationResult& result, const std::string& raw_report,
                                     const std::string& correlation_id) override;
  bool loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                    std::string* correlation_id) override;
  void saveEcuReportCounter(const Uptane::EcuSerial& ecu_serial, int64_t counter) override;
  bool loadEcuReportCounter(std::vector<std::pair<Uptane::EcuSerial, int64_t>>* results) override;
  void clearInstallationResults() override;

  bool checkAvailableDiskSpace(uint64_t required_bytes) const override;

  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const Uptane::Target& target) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const Uptane::Target& target) override;
  boost::optional<std::pair<size_t, std::string>> checkTargetFile(const Uptane::Target& target) const override;
  std::vector<Uptane::Target> getTargetFiles() override;
  void removeTargetFile(const std::string& target_name) override;
  void cleanUp() override;
  StorageType type() override { return StorageType::kSqlite; };

 private:
  boost::filesystem::path images_path_{sqldb_path_.parent_path() / "images"};

  void cleanMetaVersion(Uptane::RepositoryType repo, const Uptane::Role& role);
};

#endif  // SQLSTORAGE_H_
