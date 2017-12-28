#include "invstorage.h"
#include "fsstorage.h"
#include "sqlstorage.h"

#include "logger.h"
#include "utils.h"

void INvStorage::importSimple(store_data_t store_func, load_data_t load_func,
                              boost::filesystem::path imported_data_path) {
  if (!(this->*load_func)(NULL) && !imported_data_path.empty()) {
    if (!boost::filesystem::exists(imported_data_path)) {
      LOGGER_LOG(LVL_error, "Couldn't import data: " << imported_data_path << " doesn't exist.");
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
      LOGGER_LOG(LVL_error, "Couldn't import data: " << imported_data_path << " doesn't exist.");
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

boost::shared_ptr<INvStorage> INvStorage::newStorage(const StorageConfig& config) {
  switch (config.type) {
    case kSqlite:
      return boost::shared_ptr<INvStorage>(new SQLStorage(config));
    case kFile:
    default:
      return boost::shared_ptr<INvStorage>(new FSStorage(config));
  }
}