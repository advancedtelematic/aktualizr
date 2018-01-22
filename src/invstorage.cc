#include "invstorage.h"
#include "fsstorage.h"
#include "sqlstorage.h"

#include <boost/smart_ptr/make_shared.hpp>

#include "logging.h"
#include "utils.h"

void INvStorage::importSimple(store_data_t store_func, load_data_t load_func,
                              boost::filesystem::path imported_data_path) {
  if (!(this->*load_func)(NULL) && !imported_data_path.empty()) {
    if (!boost::filesystem::exists(imported_data_path)) {
      LOG_ERROR << "Couldn't import data: " << imported_data_path << " doesn't exist.";
    }
    std::string content = Utils::readFile(imported_data_path.string());
    (this->*store_func)(content);
  }
}

void INvStorage::importUpdateSimple(store_data_t store_func, load_data_t load_func,
                                    boost::filesystem::path imported_data_path) {
  std::string prev_content;
  std::string content;
  bool update = false;
  if (!(this->*load_func)(&prev_content)) {
    update = true;
  } else {
    content = Utils::readFile(imported_data_path.string());
    if (Crypto::sha256digest(content) != Crypto::sha256digest(prev_content)) {
      update = true;
    }
  }

  if (update && !imported_data_path.empty()) {
    if (!boost::filesystem::exists(imported_data_path)) {
      LOG_ERROR << "Couldn't import data: " << imported_data_path << " doesn't exist.";
    }
    if (content.empty()) content = Utils::readFile(imported_data_path.string());
    (this->*store_func)(content);
  }
}

void INvStorage::importData(const ImportConfig& import_config) {
  importSimple(&INvStorage::storePrimaryPublic, &INvStorage::loadPrimaryPublic, import_config.uptane_public_key_path);
  importSimple(&INvStorage::storePrimaryPrivate, &INvStorage::loadPrimaryPrivate,
               import_config.uptane_private_key_path);
  // root CA certificate can be updated
  importUpdateSimple(&INvStorage::storeTlsCa, &INvStorage::loadTlsCa, import_config.tls_cacert_path);
  importSimple(&INvStorage::storeTlsCert, &INvStorage::loadTlsCert, import_config.tls_clientcert_path);
  importSimple(&INvStorage::storeTlsPkey, &INvStorage::loadTlsPkey, import_config.tls_pkey_path);
}

boost::shared_ptr<INvStorage> INvStorage::newStorage(const StorageConfig& config, const P11Config& p11_config,
                                                     const boost::filesystem::path& path) {
  (void)path;
  switch (config.type) {
    case kSqlite:
      if (!boost::filesystem::exists(config.sqldb_path)) {
        StorageConfig old_config;
        old_config.type = kFileSystem;
        old_config.path = path;
        old_config.uptane_metadata_path = "metadata";
        old_config.uptane_private_key_path = "ecukey.der";
        old_config.uptane_public_key_path = "ecukey.pub";
        old_config.tls_cacert_path = "root.crt";
        old_config.tls_pkey_path = "pkey.pem";
        old_config.tls_clientcert_path = "client.pem";

        boost::shared_ptr<INvStorage> sql_storage = boost::make_shared<SQLStorage>(config);
        boost::shared_ptr<INvStorage> fs_storage = boost::make_shared<FSStorage>(old_config);
        INvStorage::FSSToSQLS(fs_storage, sql_storage);
        return sql_storage;
      }
      return boost::shared_ptr<INvStorage>(new SQLStorage(config, p11_config));
    case kFileSystem:
    default:
      return boost::shared_ptr<INvStorage>(new FSStorage(config, p11_config));
  }
}

void INvStorage::FSSToSQLS(const boost::shared_ptr<INvStorage>& fs_storage,
                           boost::shared_ptr<INvStorage>& sql_storage) {
  std::string public_key;
  std::string private_key;
  if (fs_storage->loadPrimaryKeys(&public_key, &private_key)) sql_storage->storePrimaryKeys(public_key, private_key);

  std::string ca;
  std::string cert;
  std::string pkey;
  if (fs_storage->loadTlsCreds(&ca, &cert, &pkey)) sql_storage->storeTlsCreds(ca, cert, pkey);

  std::string device_id;
  if (fs_storage->loadDeviceId(&device_id)) sql_storage->storeDeviceId(device_id);

  std::vector<std::pair<std::string, std::string> > serials;
  if (fs_storage->loadEcuSerials(&serials)) sql_storage->storeEcuSerials(serials);

  if (fs_storage->loadEcuRegistered()) {
    sql_storage->storeEcuRegistered();
  }

  std::map<std::string, std::string> installed_versions;
  if (fs_storage->loadInstalledVersions(&installed_versions)) sql_storage->storeInstalledVersions(installed_versions);

#ifdef BUILD_OSTREE
  Uptane::MetaPack metadata;
  if (fs_storage->loadMetadata(&metadata)) {
    sql_storage->storeMetadata(metadata);
  }
#endif

  // if everything is ok, remove old files.
  fs_storage->clearPrimaryKeys();
  fs_storage->clearTlsCreds();
  fs_storage->clearDeviceId();
  fs_storage->clearEcuSerials();
  fs_storage->clearEcuRegistered();
  fs_storage->clearInstalledVersions();
#ifdef BUILD_OSTREE
  fs_storage->clearMetadata();
#endif
  fs_storage->cleanUp();
}
