#include "invstorage.h"
#include "fsstorage.h"
#include "sqlstorage.h"

#include "logging/logging.h"
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
  writeOption(out_stream, sqldb_path, "sqldb_path");
  writeOption(out_stream, uptane_metadata_path, "uptane_metadata_path");
  writeOption(out_stream, uptane_private_key_path, "uptane_private_key_path");
  writeOption(out_stream, uptane_public_key_path, "uptane_public_key_path");
  writeOption(out_stream, tls_cacert_path, "tls_cacert_path");
  writeOption(out_stream, tls_pkey_path, "tls_pkey_path");
  writeOption(out_stream, tls_clientcert_path, "tls_clientcert_path");
}

void ImportConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(uptane_private_key_path, "uptane_private_key_path", pt);
  CopyFromConfig(uptane_public_key_path, "uptane_public_key_path", pt);
  CopyFromConfig(tls_cacert_path, "tls_cacert_path", pt);
  CopyFromConfig(tls_pkey_path, "tls_pkey_path", pt);
  CopyFromConfig(tls_clientcert_path, "tls_clientcert_path", pt);
}

void ImportConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, uptane_private_key_path, "uptane_private_key_path");
  writeOption(out_stream, uptane_public_key_path, "uptane_public_key_path");
  writeOption(out_stream, tls_cacert_path, "tls_cacert_path");
  writeOption(out_stream, tls_pkey_path, "tls_pkey_path");
  writeOption(out_stream, tls_clientcert_path, "tls_clientcert_path");
}

void INvStorage::importSimple(store_data_t store_func, load_data_t load_func,
                              boost::filesystem::path imported_data_path) {
  if (!(this->*load_func)(nullptr) && !imported_data_path.empty()) {
    if (!boost::filesystem::exists(imported_data_path)) {
      LOG_ERROR << "Couldn't import data: " << imported_data_path << " doesn't exist.";
      return;
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
      return;
    }
    if (content.empty()) {
      content = Utils::readFile(imported_data_path.string());
    }
    (this->*store_func)(content);
  }
}

void INvStorage::importPrimaryKeys(const boost::filesystem::path& import_pubkey_path,
                                   const boost::filesystem::path& import_privkey_path) {
  if (loadPrimaryKeys(nullptr, nullptr) || import_pubkey_path.empty() || import_privkey_path.empty()) {
    return;
  }
  if (!boost::filesystem::exists(import_pubkey_path)) {
    LOG_ERROR << "Couldn't import data: " << import_pubkey_path << " doesn't exist.";
    return;
  }
  if (!boost::filesystem::exists(import_privkey_path)) {
    LOG_ERROR << "Couldn't import data: " << import_privkey_path << " doesn't exist.";
    return;
  }
  std::string pub_content = Utils::readFile(import_pubkey_path.string());
  std::string priv_content = Utils::readFile(import_privkey_path.string());
  storePrimaryKeys(pub_content, priv_content);
}

void INvStorage::importData(const ImportConfig& import_config) {
  importPrimaryKeys(import_config.uptane_public_key_path, import_config.uptane_private_key_path);
  // root CA certificate can be updated
  importUpdateSimple(&INvStorage::storeTlsCa, &INvStorage::loadTlsCa, import_config.tls_cacert_path);
  importSimple(&INvStorage::storeTlsCert, &INvStorage::loadTlsCert, import_config.tls_clientcert_path);
  importSimple(&INvStorage::storeTlsPkey, &INvStorage::loadTlsPkey, import_config.tls_pkey_path);
}

std::shared_ptr<INvStorage> INvStorage::newStorage(const StorageConfig& config, const boost::filesystem::path& path) {
  switch (config.type) {
    case StorageType::kSqlite:
      if (!boost::filesystem::exists(config.sqldb_path)) {
        StorageConfig old_config;
        old_config.type = StorageType::kFileSystem;
        old_config.path = path;

        std::shared_ptr<INvStorage> sql_storage = std::make_shared<SQLStorage>(config);
        std::shared_ptr<INvStorage> fs_storage = std::make_shared<FSStorage>(old_config, true);
        INvStorage::FSSToSQLS(fs_storage, sql_storage);
        return sql_storage;
      }
      return std::make_shared<SQLStorage>(config);
    case StorageType::kFileSystem:
    default:
      return std::make_shared<FSStorage>(config);
  }
}

void INvStorage::FSSToSQLS(const std::shared_ptr<INvStorage>& fs_storage, std::shared_ptr<INvStorage>& sql_storage) {
  std::string public_key;
  std::string private_key;
  if (fs_storage->loadPrimaryKeys(&public_key, &private_key)) {
    sql_storage->storePrimaryKeys(public_key, private_key);
  }

  std::string ca;
  std::string cert;
  std::string pkey;
  if (fs_storage->loadTlsCreds(&ca, &cert, &pkey)) {
    sql_storage->storeTlsCreds(ca, cert, pkey);
  }

  std::string device_id;
  if (fs_storage->loadDeviceId(&device_id)) {
    sql_storage->storeDeviceId(device_id);
  }

  EcuSerials serials;
  if (fs_storage->loadEcuSerials(&serials)) {
    sql_storage->storeEcuSerials(serials);
  }

  if (fs_storage->loadEcuRegistered()) {
    sql_storage->storeEcuRegistered();
  }

  std::vector<Uptane::Target> installed_versions;
  std::string current_hash = fs_storage->loadInstalledVersions(&installed_versions);
  if (installed_versions.size() != 0u) {
    sql_storage->storeInstalledVersions(installed_versions, current_hash);
  }

  // migrate latest versions of all metadata
  for (auto role : Uptane::Role::Roles()) {
    std::string meta;
    for (auto repo : {Uptane::RepositoryType::Director, Uptane::RepositoryType::Images}) {
      if (fs_storage->loadRole(&meta, repo, role)) {
        int version = Uptane::extractVersionUntrusted(meta);
        if (version >= 0) {
          sql_storage->storeRole(meta, repo, role, Uptane::Version(version));
        }
      }
    }
  }
  // additionally migrate the whole root metadata chain
  std::string latest_root;
  for (auto repo : {Uptane::RepositoryType::Director, Uptane::RepositoryType::Images}) {
    if (fs_storage->loadRole(&latest_root, Uptane::RepositoryType::Director, Uptane::Role::Root())) {
      int latest_version = Uptane::extractVersionUntrusted(latest_root);
      for (int version = 0; version < latest_version; ++version) {
        std::string root;
        if (fs_storage->loadRole(&root, repo, Uptane::Role::Root(), Uptane::Version(version))) {
          sql_storage->storeRole(root, repo, Uptane::Role::Root(), Uptane::Version(version));
        }
      }
    }
  }

  // if everything is ok, remove old files.
  fs_storage->clearPrimaryKeys();
  fs_storage->clearTlsCreds();
  fs_storage->clearDeviceId();
  fs_storage->clearEcuSerials();
  fs_storage->clearEcuRegistered();
  fs_storage->clearInstalledVersions();
  fs_storage->clearMetadata();
  fs_storage->cleanUp();
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
