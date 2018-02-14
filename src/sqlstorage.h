#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <boost/filesystem.hpp>

#include <sqlite3.h>

#include "config.h"
#include "invstorage.h"

// See docs/schema-migrations.adoc
const int kSqlSchemaVersion = 3;

enum SQLReqId { kSqlGetSimple, kSqlGetTable };

class SQLStorage : public INvStorage {
 public:
  friend class SQLTargetWHandle;
  friend class SQLTargetRHandle;
  SQLStorage(const StorageConfig& config);
  virtual ~SQLStorage();
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key);
  virtual void storePrimaryPublic(const std::string& public_key);
  virtual void storePrimaryPrivate(const std::string& private_key);
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  virtual bool loadPrimaryPublic(std::string* public_key);
  virtual bool loadPrimaryPrivate(std::string* private_key);
  virtual void clearPrimaryKeys();

  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual void storeTlsCa(const std::string& ca);
  virtual void storeTlsCert(const std::string& cert);
  virtual void storeTlsPkey(const std::string& pkey);
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void clearTlsCreds();
  virtual bool loadTlsCa(std::string* ca);
  virtual bool loadTlsCert(std::string* cert);
  virtual bool loadTlsPkey(std::string* cert);
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
  virtual void clearMetadata();
  virtual void storeDeviceId(const std::string& device_id);
  virtual bool loadDeviceId(std::string* device_id);
  virtual void clearDeviceId();
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials);
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials);
  virtual void clearEcuSerials();
  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus);
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus);
  virtual void clearMisconfiguredEcus();
  virtual void storeEcuRegistered();
  virtual bool loadEcuRegistered();
  virtual void clearEcuRegistered();
  virtual void storeInstalledVersions(const std::map<std::string, InstalledVersion>& installed_versions);
  virtual bool loadInstalledVersions(std::map<std::string, InstalledVersion>* installed_versions);
  virtual void clearInstalledVersions();
  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) override;
  void removeTargetFile(const std::string& filename) override;
  virtual void cleanUp();
  virtual StorageType type() { return kSqlite; };

  std::string getTableSchemaFromDb(const std::string& tablename);

  bool dbMigrate();
  bool dbInit();

 private:
  // request info
  SQLReqId request;
  std::map<std::string, std::string> req_params;
  std::map<std::string, std::string> req_response;
  std::vector<std::map<std::string, std::string> > req_response_table;

  static int callback(void* instance_, int numcolumns, char** values, char** columns);
  int getVersion();  // non-negative integer on success or -1 on error

  bool loadTlsCommon(std::string* data,
                     const boost::filesystem::path& path_in);  // TODO: delete after implementation is ready
};

#endif  // SQLSTORAGE_H_
