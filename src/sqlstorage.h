#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <boost/filesystem.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/tokenizer.hpp>

#include <sqlite3.h>

#include "config.h"
#include "invstorage.h"

const int kSqlSchemaVersion = 2;

enum SQLReqId { kSqlGetSimple, kSqlGetTable };

typedef boost::tokenizer<boost::char_separator<char> > sql_tokenizer;

class SQLite3Guard {
 public:
  sqlite3* get() { return handler; }
  int get_rc() { return rc; }

  SQLite3Guard(const char* path) { rc = sqlite3_open(path, &handler); }
  ~SQLite3Guard() { sqlite3_close(handler); }

 private:
  sqlite3* handler;
  int rc;
};

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
  virtual void storeInstalledVersions(const std::map<std::string, std::string>& installed_versions);
  virtual bool loadInstalledVersions(std::map<std::string, std::string>* installed_versions);
  virtual void clearInstalledVersions();
  std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                           size_t size) override;
  std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) override;
  void removeTargetFile(const std::string& filename) override;
  virtual void cleanUp();
  virtual StorageType type() { return kSqlite; };

  bool dbMigrate();
  bool dbCheck();
  bool dbInit();

 private:
  const StorageConfig& config_;
  // request info
  SQLReqId request;
  std::map<std::string, std::string> req_params;
  std::map<std::string, std::string> req_response;
  std::vector<std::map<std::string, std::string> > req_response_table;

  boost::movelib::unique_ptr<std::map<std::string, std::string> > parseSchema(int version);
  bool tableSchemasEqual(const std::string& left, const std::string& right);
  std::string getTableSchemaFromDb(const std::string& tablename);

  static int callback(void* instance_, int numcolumns, char** values, char** columns);
  int getVersion();  // non-negative integer on success or -1 on error

  bool loadTlsCommon(std::string* data,
                     const boost::filesystem::path& path_in);  // TODO: delete after implementation is ready
};

#endif  // SQLSTORAGE_H_
