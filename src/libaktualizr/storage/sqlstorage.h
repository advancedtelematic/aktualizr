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
  explicit SQLStorage(const StorageConfig& config, bool readonly, StorageClient storage_client = StorageClient::kUptane);
  ~SQLStorage() override = default;
  void storePrimaryKeys(const std::string& public_key, const std::string& private_key) override;
  bool loadPrimaryKeys(std::string* public_key, std::string* private_key) const override;
  bool loadPrimaryPublic(std::string* public_key) const override;
  bool loadPrimaryPrivate(std::string* private_key) const override;
  void clearPrimaryKeys() override;

  void saveSecondaryInfo(const Uptane::EcuSerial& ecu_serial, const std::string& sec_type,
                         const PublicKey& public_key) override;
  void saveSecondaryData(const Uptane::EcuSerial& ecu_serial, const std::string& data) override;
  bool loadSecondaryInfo(const Uptane::EcuSerial& ecu_serial, SecondaryInfo* secondary) const override;
  bool loadSecondariesInfo(std::vector<SecondaryInfo>* secondaries) const override;

  void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) override;
  void storeTlsCa(const std::string& ca) override;
  void storeTlsCert(const std::string& cert) override;
  void storeTlsPkey(const std::string& pkey) override;
  bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) const override;
  void clearTlsCreds() override;
  bool loadTlsCa(std::string* ca) const override;
  bool loadTlsCert(std::string* cert) const override;
  bool loadTlsPkey(std::string* pkey) const override;

  void storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) override;
  bool loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) const override;
  void storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) override;
  bool loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) const override;
  void clearNonRootMeta(Uptane::RepositoryType repo) override;
  void clearMetadata() override;
  void storeDelegation(const std::string& data, Uptane::Role role) override;
  bool loadDelegation(std::string* data, Uptane::Role role) const override;
  bool loadAllDelegations(std::vector<std::pair<Uptane::Role, std::string>>& data) const override;
  void deleteDelegation(Uptane::Role role) override;
  void clearDelegations() override;

  void storeDeviceId(const std::string& device_id) override;
  bool loadDeviceId(std::string* device_id) const override;
  void clearDeviceId() override;
  void storeEcuSerials(const EcuSerials& serials) override;
  bool loadEcuSerials(EcuSerials* serials) const override;
  void clearEcuSerials() override;
  void storeCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, const std::string& manifest) override;
  bool loadCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, std::string* manifest) const override;
  void saveMisconfiguredEcu(const MisconfiguredEcu& ecu) override;
  bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) const override;
  void clearMisconfiguredEcus() override;
  void storeEcuRegistered() override;
  bool loadEcuRegistered() const override;
  void clearEcuRegistered() override;
  void storeNeedReboot() override;
  bool loadNeedReboot(bool* need_reboot) const override;
  void clearNeedReboot() override;
  void saveInstalledVersion(const std::string& ecu_serial, const Uptane::Target& target,
                            InstalledVersionUpdateMode update_mode) override;
  bool loadInstalledVersions(const std::string& ecu_serial, boost::optional<Uptane::Target>* current_version,
                             boost::optional<Uptane::Target>* pending_version) const override;
  bool loadInstallationLog(const std::string& ecu_serial, std::vector<Uptane::Target>* log,
                           bool only_installed) const override;
  bool hasPendingInstall() override;
  void getPendingEcus(std::vector<std::pair<Uptane::EcuSerial, Hash>>* pendingEcus) override;
  void clearInstalledVersions() override;

  void saveEcuInstallationResult(const Uptane::EcuSerial& ecu_serial, const data::InstallationResult& result) override;
  bool loadEcuInstallationResults(
      std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) const override;
  void storeDeviceInstallationResult(const data::InstallationResult& result, const std::string& raw_report,
                                     const std::string& correlation_id) override;
  bool storeDeviceInstallationRawReport(const std::string& raw_report) override;
  bool loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                    std::string* correlation_id) const override;
  void saveEcuReportCounter(const Uptane::EcuSerial& ecu_serial, int64_t counter) override;
  bool loadEcuReportCounter(std::vector<std::pair<Uptane::EcuSerial, int64_t>>* results) const override;
  void saveReportEvent(const Json::Value& json_value) override;
  bool loadReportEvents(Json::Value* report_array, int64_t* id_max) const override;
  void deleteReportEvents(int64_t id_max) override;
  void clearInstallationResults() override;

  void storeDeviceDataHash(const std::string& data_type, const std::string& hash) override;
  bool loadDeviceDataHash(const std::string& data_type, std::string* hash) const override;
  void clearDeviceData() override;

  void storeTargetFilename(const std::string& targetname, const std::string& filename) const override;
  std::string getTargetFilename(const std::string& targetname) const override;
  std::vector<std::string> getAllTargetNames() const override;
  void deleteTargetInfo(const std::string& targetname) const override;

  void cleanUp() override;
  StorageType type() override { return StorageType::kSqlite; };

 private:
  void cleanMetaVersion(Uptane::RepositoryType repo, const Uptane::Role& role);
};

#endif  // SQLSTORAGE_H_
