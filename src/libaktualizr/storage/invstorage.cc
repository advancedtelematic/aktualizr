#include "invstorage.h"

#include <unistd.h>

#include "fsstorage_read.h"
#include "logging/logging.h"
#include "sqlstorage.h"
#include "utilities/utils.h"

std::ostream& operator<<(std::ostream& os, const StorageType stype) {
  std::string stype_str;
  switch (stype) {
    case StorageType::kFileSystem:
      stype_str = "filesystem";
      break;
    case StorageType::kSqlite:
      stype_str = "sqlite";
      break;
    default:
      stype_str = "unknown";
      break;
  }
  os << '"' << stype_str << '"';
  return os;
}

void StorageConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(type, "type", pt);
  CopyFromConfig(path, "path", pt);
  CopyFromConfig(sqldb_path, "sqldb_path", pt);
  CopyFromConfig(uptane_metadata_path, "uptane_metadata_path", pt);
  CopyFromConfig(uptane_private_key_path, "uptane_private_key_path", pt);
  CopyFromConfig(uptane_public_key_path, "uptane_public_key_path", pt);
  CopyFromConfig(tls_cacert_path, "tls_cacert_path", pt);
  CopyFromConfig(tls_pkey_path, "tls_pkey_path", pt);
  CopyFromConfig(tls_clientcert_path, "tls_clientcert_path", pt);
}

void StorageConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, type, "type");
  writeOption(out_stream, path, "path");
  writeOption(out_stream, sqldb_path.get(""), "sqldb_path");
  writeOption(out_stream, uptane_metadata_path.get(""), "uptane_metadata_path");
  writeOption(out_stream, uptane_private_key_path.get(""), "uptane_private_key_path");
  writeOption(out_stream, uptane_public_key_path.get(""), "uptane_public_key_path");
  writeOption(out_stream, tls_cacert_path.get(""), "tls_cacert_path");
  writeOption(out_stream, tls_pkey_path.get(""), "tls_pkey_path");
  writeOption(out_stream, tls_clientcert_path.get(""), "tls_clientcert_path");
}

void ImportConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(base_path, "base_path", pt);
  CopyFromConfig(uptane_private_key_path, "uptane_private_key_path", pt);
  CopyFromConfig(uptane_public_key_path, "uptane_public_key_path", pt);
  CopyFromConfig(tls_cacert_path, "tls_cacert_path", pt);
  CopyFromConfig(tls_pkey_path, "tls_pkey_path", pt);
  CopyFromConfig(tls_clientcert_path, "tls_clientcert_path", pt);
}

void ImportConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, base_path, "base_path");
  writeOption(out_stream, uptane_private_key_path.get(""), "uptane_private_key_path");
  writeOption(out_stream, uptane_public_key_path.get(""), "uptane_public_key_path");
  writeOption(out_stream, tls_cacert_path.get(""), "tls_cacert_path");
  writeOption(out_stream, tls_pkey_path.get(""), "tls_pkey_path");
  writeOption(out_stream, tls_clientcert_path.get(""), "tls_clientcert_path");
}

void INvStorage::importSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                              const BasedPath& imported_data_path) {
  if (!(this->*load_func)(nullptr) && !imported_data_path.empty()) {
    boost::filesystem::path abs_path = imported_data_path.get(base_path);
    if (!boost::filesystem::exists(abs_path)) {
      LOG_ERROR << "Couldn't import data: " << abs_path << " doesn't exist.";
      return;
    }
    std::string content = Utils::readFile(abs_path.string());
    (this->*store_func)(content);
  }
}

void INvStorage::importUpdateSimple(const boost::filesystem::path& base_path, store_data_t store_func,
                                    load_data_t load_func, const BasedPath& imported_data_path) {
  std::string prev_content;
  std::string content;
  bool update = false;
  if (!(this->*load_func)(&prev_content)) {
    update = true;
  } else if (!imported_data_path.empty()) {
    content = Utils::readFile(imported_data_path.get(base_path).string());
    if (Crypto::sha256digest(content) != Crypto::sha256digest(prev_content)) {
      update = true;
    }
  }

  if (update && !imported_data_path.empty()) {
    boost::filesystem::path abs_path = imported_data_path.get(base_path);
    if (!boost::filesystem::exists(abs_path)) {
      LOG_ERROR << "Couldn't import data: " << abs_path << " doesn't exist.";
      return;
    }
    if (content.empty()) {
      content = Utils::readFile(abs_path.string());
    }
    (this->*store_func)(content);
  }
}

void INvStorage::importPrimaryKeys(const boost::filesystem::path& base_path, const BasedPath& import_pubkey_path,
                                   const BasedPath& import_privkey_path) {
  if (loadPrimaryKeys(nullptr, nullptr) || import_pubkey_path.empty() || import_privkey_path.empty()) {
    return;
  }
  boost::filesystem::path pubkey_abs_path = import_pubkey_path.get(base_path);
  boost::filesystem::path privkey_abs_path = import_privkey_path.get(base_path);
  if (!boost::filesystem::exists(pubkey_abs_path)) {
    LOG_ERROR << "Couldn't import data: " << pubkey_abs_path << " doesn't exist.";
    return;
  }
  if (!boost::filesystem::exists(privkey_abs_path)) {
    LOG_ERROR << "Couldn't import data: " << privkey_abs_path << " doesn't exist.";
    return;
  }
  std::string pub_content = Utils::readFile(pubkey_abs_path.string());
  std::string priv_content = Utils::readFile(privkey_abs_path.string());
  storePrimaryKeys(pub_content, priv_content);
}

void INvStorage::importData(const ImportConfig& import_config) {
  importPrimaryKeys(import_config.base_path, import_config.uptane_public_key_path,
                    import_config.uptane_private_key_path);
  // root CA certificate can be updated
  importUpdateSimple(import_config.base_path, &INvStorage::storeTlsCa, &INvStorage::loadTlsCa,
                     import_config.tls_cacert_path);
  importSimple(import_config.base_path, &INvStorage::storeTlsCert, &INvStorage::loadTlsCert,
               import_config.tls_clientcert_path);
  importSimple(import_config.base_path, &INvStorage::storeTlsPkey, &INvStorage::loadTlsPkey,
               import_config.tls_pkey_path);
}

std::shared_ptr<INvStorage> INvStorage::newStorage(const StorageConfig& config, const bool readonly) {
  switch (config.type) {
    case StorageType::kSqlite: {
      boost::filesystem::path db_path = config.sqldb_path.get(config.path);
      if (!boost::filesystem::exists(db_path) && boost::filesystem::exists(config.path)) {
        if (access(config.path.c_str(), R_OK | W_OK | X_OK) != 0) {
          throw StorageException(std::string("Cannot read prior filesystem configuration from ") +
                                 config.path.string() + " due to insufficient permissions.");
        }

        StorageConfig old_config = config;
        old_config.type = StorageType::kFileSystem;
        old_config.path = config.path;

        FSStorageRead fs_storage(old_config);
        bool check_exists = true;
        if (INvStorage::FSSToSQLS(fs_storage, nullptr, check_exists)) {
          LOG_INFO << "Found existing FS storage. Starting FS to SQL storage migration.";
          if (readonly) {
            throw StorageException("Migration from FS is not possible because of readonly database.");
          }
          auto sql_storage = std::make_shared<SQLStorage>(config, readonly);
          check_exists = false;
          INvStorage::FSSToSQLS(fs_storage, sql_storage, check_exists);
          return sql_storage;
        }
      }
      if (!boost::filesystem::exists(db_path)) {
        LOG_INFO << "No storage found. Bootstrapping new SQL storage at: " << db_path;
      } else {
        LOG_INFO << "Use existing SQL storage: " << db_path;
      }
      return std::make_shared<SQLStorage>(config, readonly);
    }
    case StorageType::kFileSystem:
    default:
      throw std::runtime_error("FSStorage has been removed in recent versions of aktualizr, please use SQLStorage");
  }
}

bool INvStorage::FSSToSQLS(FSStorageRead& fs_storage, const std::shared_ptr<SQLStorage>& sql_storage,
                           const bool check_exists) {
  std::string public_key;
  std::string private_key;
  if (fs_storage.loadPrimaryKeys(&public_key, &private_key)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storePrimaryKeys(public_key, private_key);
    }
  }

  std::string ca;
  if (fs_storage.loadTlsCa(&ca)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeTlsCa(ca);
    }
  }

  std::string cert;
  if (fs_storage.loadTlsCert(&cert)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeTlsCert(cert);
    }
  }

  std::string pkey;
  if (fs_storage.loadTlsPkey(&pkey)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeTlsPkey(pkey);
    }
  }

  std::string device_id;
  if (fs_storage.loadDeviceId(&device_id)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeDeviceId(device_id);
    }
  }

  EcuSerials serials;
  if (fs_storage.loadEcuSerials(&serials)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeEcuSerials(serials);
    }
  }

  if (fs_storage.loadEcuRegistered()) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeEcuRegistered();
    }
  }

  std::vector<MisconfiguredEcu> ecus;
  if (fs_storage.loadMisconfiguredEcus(&ecus)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeMisconfiguredEcus(ecus);
    }
  }

  data::OperationResult res;
  if (fs_storage.loadInstallationResult(&res)) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeInstallationResult(res);
    }
  }

  std::vector<Uptane::Target> installed_versions;
  std::string current_hash = fs_storage.loadInstalledVersions(&installed_versions);
  if (installed_versions.size() != 0u) {
    if (check_exists) {
      return true;
    } else {
      sql_storage->storeInstalledVersions(installed_versions, current_hash);
    }
  }

  // migrate latest versions of all metadata
  for (auto role : Uptane::Role::Roles()) {
    if (role == Uptane::Role::Root()) {
      continue;
    }

    std::string meta;
    for (auto repo : {Uptane::RepositoryType::Director, Uptane::RepositoryType::Images}) {
      if (fs_storage.loadNonRoot(&meta, repo, role)) {
        if (check_exists) {
          return true;
        } else {
          sql_storage->storeNonRoot(meta, repo, role);
        }
      }
    }
  }
  // additionally migrate the whole root metadata chain
  std::string latest_root;
  for (auto repo : {Uptane::RepositoryType::Director, Uptane::RepositoryType::Images}) {
    if (fs_storage.loadLatestRoot(&latest_root, Uptane::RepositoryType::Director)) {
      int latest_version = Uptane::extractVersionUntrusted(latest_root);
      for (int version = 0; version <= latest_version; ++version) {
        std::string root;
        if (fs_storage.loadRoot(&root, repo, Uptane::Version(version))) {
          if (check_exists) {
            return true;
          } else {
            sql_storage->storeRoot(root, repo, Uptane::Version(version));
          }
        }
      }
    }
  }

  if (!check_exists) {
    // if everything is ok, remove old files.
    fs_storage.cleanUpAll();
  }
  return false;
}

void INvStorage::saveInstalledVersion(const Uptane::Target& target) {
  std::vector<Uptane::Target> versions;
  std::string new_current_hash;
  loadInstalledVersions(&versions);
  if (std::find(versions.begin(), versions.end(), target) == versions.end()) {
    versions.push_back(target);
  }
  storeInstalledVersions(versions, target.sha256Hash());
}
