#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <sqlite3.h>

#include "invstorage.h"
#include "sql_utils.h"

extern const std::vector<std::string> schema_migrations;
extern const std::string current_schema;
extern const int current_schema_version;

enum class SQLReqId { kGetSimple, kGetTable };
enum class DbVersion : int32_t { kEmpty = -1, kInvalid = -2 };

class SQLStorage : public INvStorage {
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
  bool loadInstalledVersions(const std::string& ecu_serial, std::vector<Uptane::Target>* installed_versions,
                             size_t* current_version, size_t* pending_version) override;
  bool hasPendingInstall() override;
  void clearInstalledVersions() override;
  void storeInstallationResult(const data::OperationResult& result) override;
  bool loadInstallationResult(data::OperationResult* result) override;
  void clearInstallationResult() override;

  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const Uptane::Target& target) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const Uptane::Target& target) override;
  boost::optional<size_t> checkTargetFile(const Uptane::Target& target) const override;
  void removeTargetFile(const std::string& filename) override;
  void cleanUp() override;
  StorageType type() override { return StorageType::kSqlite; };

  std::string getTableSchemaFromDb(const std::string& tablename);

  bool dbMigrate();
  DbVersion getVersion();  // non-negative integer on success or -1 on error
  boost::filesystem::path dbPath() const;

 private:
  SQLite3Guard dbConnection() const;
  // request info
  void cleanMetaVersion(Uptane::RepositoryType repo, Uptane::Role role);
  bool readonly_{false};
};

#endif  // SQLSTORAGE_H_
