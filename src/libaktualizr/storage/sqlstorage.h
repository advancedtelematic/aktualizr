#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <boost/filesystem.hpp>

#include <sqlite3.h>

#include "invstorage.h"

extern const std::vector<std::string> schema_migrations;
extern const std::string current_schema;

enum class SQLReqId { GetSimple, GetTable };
enum class DbVersion : int32_t { Empty = -1, Invalid = -2 };

class SQLStorage : public INvStorage {
 public:
  friend class SQLTargetWHandle;
  friend class SQLTargetRHandle;
  explicit SQLStorage(const StorageConfig& config);
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
  void storeMetadata(const Uptane::RawMetaPack& metadata) override;
  bool loadMetadata(Uptane::RawMetaPack* metadata) override;
  void clearMetadata() override;
  void storeUncheckedMetadata(const Uptane::RawMetaPack& metadata) override;
  bool loadUncheckedMetadata(Uptane::RawMetaPack* metadata) override;
  void clearUncheckedMetadata() override;
  void storeRoot(bool director, const std::string& root, Uptane::Version version) override;
  bool loadRoot(bool director, std::string* root, Uptane::Version version) override;
  void storeUncheckedRoot(bool director, const std::string& root, Uptane::Version version) override;
  bool loadUncheckedRoot(bool director, std::string* root, Uptane::Version version) override;
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
  void storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                              const std::string& current_hash) override;
  std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) override;
  void clearInstalledVersions() override;
  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) override;
  void removeTargetFile(const std::string& filename) override;
  void cleanUp() override;
  StorageType type() override { return StorageType::Sqlite; };

  std::string getTableSchemaFromDb(const std::string& tablename);

  bool dbMigrate();
  bool dbInit();
  DbVersion getVersion();  // non-negative integer on success or -1 on error

 private:
  // request info
  SQLReqId request{};
  std::map<std::string, std::string> req_params;
  std::map<std::string, std::string> req_response;
  std::vector<std::map<std::string, std::string> > req_response_table;

  static int callback(void* instance_, int numcolumns, char** values, char** columns);

  void storeMetadataCommon(const Uptane::RawMetaPack& metadata, const std::string& tablename);
  bool loadMetadataCommon(Uptane::RawMetaPack* metadata, const std::string& tablename);
  void clearMetadataCommon(const std::string& tablename);
  void storeRootCommon(bool director, const std::string& root, Uptane::Version version, const std::string& tablename);
  bool loadRootCommon(bool director, std::string* root, Uptane::Version version, const std::string& tablename);
};

#endif  // SQLSTORAGE_H_
