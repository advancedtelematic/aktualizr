#ifndef SQLSTORAGE_H_
#define SQLSTORAGE_H_

#include <sqlite3.h>

#include "config.h"
#include "invstorage.h"

enum SQLReqId {
  kSqlTableInfo,
  kSqlGetVersion,

};

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
  SQLStorage(const Config& config);
  virtual ~SQLStorage();
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key);
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key);
  virtual void clearPrimaryKeys();
  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey);
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey);
  virtual void clearTlsCreds();
  virtual bool loadTlsCa(std::string* ca);
  virtual bool loadTlsCert(std::string* cert);
  virtual bool loadTlsPkey(std::string* cert);
#ifdef BUILD_OSTREE
  virtual void storeMetadata(const Uptane::MetaPack& metadata);
  virtual bool loadMetadata(Uptane::MetaPack* metadata);
#endif  // BUILD_OSTREE
  virtual void storeDeviceId(const std::string& device_id);
  virtual bool loadDeviceId(std::string* device_id);
  virtual void clearDeviceId();
  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials);
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials);
  virtual void clearEcuSerials();
  virtual void storeEcuRegistered();
  virtual bool loadEcuRegistered();
  virtual void clearEcuRegistered();
  virtual void storeInstalledVersions(const std::string& content);
  virtual bool loadInstalledVersions(std::string* content);

  bool createFromSchema(boost::filesystem::path db_path, const Json::Value& schema);
  bool checkSchema(boost::filesystem::path db_path, const Json::Value& schema);

  bool loadTlsCommon(std::string* data, const std::string& path_in);  // TODO: delete after implementation is ready

 private:
  Config config_;
  // request info
  SQLReqId request;
  std::map<std::string, std::string> req_params;
  std::map<std::string, std::string> req_response;

  static int callback(void* instance_, int numcolumns, char** values, char** columns);
};

#endif  // SQLSTORAGE_H_
