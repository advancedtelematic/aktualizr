#include "invstorage.h"

#include <unistd.h>

#include "fsstorage_read.h"
#include "logging/logging.h"
#include "sqlstorage.h"
#include "utilities/utils.h"

void INvStorage::importSimple(const boost::filesystem::path& base_path, store_data_t store_func, load_data_t load_func,
                              const utils::BasedPath& imported_data_path, const std::string& data_name) {
  if (!(this->*load_func)(nullptr) && !imported_data_path.empty()) {
    boost::filesystem::path abs_path = imported_data_path.get(base_path);
    if (!boost::filesystem::exists(abs_path)) {
      LOG_ERROR << "Couldn't import " << data_name << ": " << abs_path << " doesn't exist.";
      return;
    }
    std::string content = Utils::readFile(abs_path.string());
    (this->*store_func)(content);
    LOG_DEBUG << "Successfully imported " << data_name << " from " << abs_path;
  }
}

void INvStorage::importUpdateSimple(const boost::filesystem::path& base_path, store_data_t store_func,
                                    load_data_t load_func, const utils::BasedPath& imported_data_path,
                                    const std::string& data_name) {
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
      LOG_ERROR << "Couldn't import " << data_name << ": " << abs_path << " doesn't exist.";
      return;
    }
    if (content.empty()) {
      content = Utils::readFile(abs_path.string());
    }
    (this->*store_func)(content);
    LOG_DEBUG << "Successfully imported " << data_name << " from " << abs_path;
  }
}

void INvStorage::importUpdateCertificate(const boost::filesystem::path& base_path,
                                         const utils::BasedPath& imported_data_path) {
  std::string prev_content;
  std::string content;
  bool update = false;
  if (!loadTlsCert(&prev_content)) {
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
      LOG_ERROR << "Couldn't import client certificate: " << abs_path << " doesn't exist.";
      return;
    }
    if (content.empty()) {
      content = Utils::readFile(abs_path.string());
    }

    // Make sure the device ID of the new cert hasn't changed.
    const std::string new_device_id = Crypto::extractSubjectCN(content);
    std::string old_device_id;
    if (!loadDeviceId(&old_device_id)) {
      LOG_DEBUG << "Unable to load previous device ID.";
    } else if (new_device_id != old_device_id) {
      LOG_WARNING << "Certificate at " << abs_path.string() << " has a CN that may be used as device ID of "
                  << new_device_id << " but the device currently is identified as " << old_device_id;
    }

    storeTlsCert(content);
    LOG_DEBUG << "Successfully imported client certificate from " << abs_path;
  }
}

void INvStorage::importPrimaryKeys(const boost::filesystem::path& base_path, const utils::BasedPath& import_pubkey_path,
                                   const utils::BasedPath& import_privkey_path) {
  if (client_ == StorageClient::kTUF) {
    LOG_DEBUG << "TUF instance, primary keys not required";
    return;
  }
  if (import_pubkey_path.empty() || import_privkey_path.empty()) {
    LOG_ERROR << "Couldn`t import data: empty path received";
    return;
  }
  if (loadPrimaryKeys(nullptr, nullptr)) {
    LOG_INFO << "Couldn`t import data: primary keys already in storage";
    return;
  }
  const boost::filesystem::path pubkey_abs_path = import_pubkey_path.get(base_path);
  const boost::filesystem::path privkey_abs_path = import_privkey_path.get(base_path);
  if (!boost::filesystem::exists(pubkey_abs_path)) {
    LOG_ERROR << "Couldn't import data: " << pubkey_abs_path << " doesn't exist.";
    return;
  }
  if (!boost::filesystem::exists(privkey_abs_path)) {
    LOG_ERROR << "Couldn't import data: " << privkey_abs_path << " doesn't exist.";
    return;
  }
  const std::string pub_content = Utils::readFile(pubkey_abs_path.string());
  const std::string priv_content = Utils::readFile(privkey_abs_path.string());
  storePrimaryKeys(pub_content, priv_content);
  LOG_DEBUG << "Successfully imported Uptane keys from " << pubkey_abs_path << " and " << privkey_abs_path;
}

void INvStorage::importInstalledVersions(const boost::filesystem::path& base_path) {
  std::vector<Uptane::Target> installed_versions;
  const boost::filesystem::path file_path = utils::BasedPath("installed_versions").get(base_path);
  loadPrimaryInstallationLog(&installed_versions, false);
  if (!installed_versions.empty()) {
    return;
  }
  size_t current_index = SIZE_MAX;
  fsReadInstalledVersions(file_path, &installed_versions, &current_index);
  if (current_index < installed_versions.size()) {
    // installed versions in legacy fs storage are all for primary
    savePrimaryInstalledVersion(installed_versions[current_index], InstalledVersionUpdateMode::kCurrent);
    boost::filesystem::remove(file_path);
    LOG_DEBUG << "Successfully imported installed versions from " << file_path;
  }
}

void INvStorage::importData(const ImportConfig& import_config) {
  importPrimaryKeys(import_config.base_path, import_config.uptane_public_key_path,
                    import_config.uptane_private_key_path);
  importUpdateCertificate(import_config.base_path, import_config.tls_clientcert_path);
  importUpdateSimple(import_config.base_path, &INvStorage::storeTlsCa, &INvStorage::loadTlsCa,
                     import_config.tls_cacert_path, "server CA certificate");
  importUpdateSimple(import_config.base_path, &INvStorage::storeTlsPkey, &INvStorage::loadTlsPkey,
                     import_config.tls_pkey_path, "client TLS key");
  importInstalledVersions(import_config.base_path);
}

std::shared_ptr<INvStorage> INvStorage::newStorage(const StorageConfig& config, const bool readonly,
                                                   StorageClient client) {
  switch (config.type) {
    case StorageType::kSqlite: {
      boost::filesystem::path db_path = config.sqldb_path.get(config.path);
      if (!boost::filesystem::exists(db_path) && FSStorageRead::FSStoragePresent(config)) {
        if (readonly) {
          throw StorageException(
              "Migration from FS is not possible because the SQL database is configured to be readonly");
        }

        LOG_INFO << "Starting FS to SQL storage migration";
        if (access(config.path.c_str(), R_OK | W_OK | X_OK) != 0) {
          throw StorageException(std::string("Cannot read prior filesystem configuration from ") +
                                 config.path.string() + " due to insufficient permissions.");
        }
        StorageConfig old_config = config;
        old_config.type = StorageType::kFileSystem;
        old_config.path = config.path;

        auto sql_storage = std::make_shared<SQLStorage>(config, readonly, client);
        FSStorageRead fs_storage(old_config);
        INvStorage::FSSToSQLS(fs_storage, *sql_storage);
        return sql_storage;
      }
      if (!boost::filesystem::exists(db_path)) {
        LOG_INFO << "Bootstrap empty SQL storage";
      } else {
        LOG_INFO << "Use existing SQL storage: " << db_path;
      }
      return std::make_shared<SQLStorage>(config, readonly, client);
    }
    case StorageType::kFileSystem:
    default:
      throw std::runtime_error("FSStorage has been removed in recent versions of aktualizr, please use SQLStorage");
  }
}

void INvStorage::FSSToSQLS(FSStorageRead& fs_storage, SQLStorage& sql_storage) {
  std::string public_key;
  std::string private_key;
  if (fs_storage.loadPrimaryKeys(&public_key, &private_key)) {
    sql_storage.storePrimaryKeys(public_key, private_key);
  }

  std::string ca;
  if (fs_storage.loadTlsCa(&ca)) {
    sql_storage.storeTlsCa(ca);
  }

  std::string cert;
  if (fs_storage.loadTlsCert(&cert)) {
    sql_storage.storeTlsCert(cert);
  }

  std::string pkey;
  if (fs_storage.loadTlsPkey(&pkey)) {
    sql_storage.storeTlsPkey(pkey);
  }

  std::string device_id;
  if (fs_storage.loadDeviceId(&device_id)) {
    sql_storage.storeDeviceId(device_id);
  }

  EcuSerials serials;
  if (fs_storage.loadEcuSerials(&serials)) {
    sql_storage.storeEcuSerials(serials);
  }

  if (fs_storage.loadEcuRegistered()) {
    sql_storage.storeEcuRegistered();
  }

  std::vector<MisconfiguredEcu> ecus;
  if (fs_storage.loadMisconfiguredEcus(&ecus)) {
    for (auto& ecu : ecus) {
      sql_storage.saveMisconfiguredEcu(ecu);
    }
  }

  std::vector<Uptane::Target> installed_versions;
  size_t current_index = SIZE_MAX;
  size_t k = 0;
  fs_storage.loadInstalledVersions(&installed_versions, &current_index);
  for (auto it = installed_versions.cbegin(); it != installed_versions.cend(); it++, k++) {
    auto mode = k == current_index ? InstalledVersionUpdateMode::kCurrent : InstalledVersionUpdateMode::kNone;
    sql_storage.savePrimaryInstalledVersion(*it, mode);
  }

  // migrate latest versions of all metadata
  for (const auto& role : Uptane::Role::Roles()) {
    if (role == Uptane::Role::Root()) {
      continue;
    }

    std::string meta;
    for (auto repo : {Uptane::RepositoryType::Director(), Uptane::RepositoryType::Image()}) {
      if (fs_storage.loadNonRoot(&meta, repo, role)) {
        sql_storage.storeNonRoot(meta, repo, role);
      }
    }
  }
  // additionally migrate the whole Root metadata chain
  std::string latest_root;
  for (auto repo : {Uptane::RepositoryType::Director(), Uptane::RepositoryType::Image()}) {
    if (fs_storage.loadLatestRoot(&latest_root, Uptane::RepositoryType::Director())) {
      int latest_version = Uptane::extractVersionUntrusted(latest_root);
      for (int version = 0; version <= latest_version; ++version) {
        std::string root;
        if (fs_storage.loadRoot(&root, repo, Uptane::Version(version))) {
          sql_storage.storeRoot(root, repo, Uptane::Version(version));
        }
      }
    }
  }

  // if everything is ok, remove old files.
  fs_storage.cleanUpAll();
}

bool INvStorage::fsReadInstalledVersions(const boost::filesystem::path& filename,
                                         std::vector<Uptane::Target>* installed_versions, size_t* current_version) {
  if (access(filename.c_str(), R_OK) != 0) {
    return false;
  }
  try {
    const Json::Value installed_versions_json = Utils::parseJSONFile(filename.string());
    std::vector<Uptane::Target> new_versions;
    size_t k = 0;
    for (auto it = installed_versions_json.begin(); it != installed_versions_json.end(); ++it, ++k) {
      if (!(*it).isObject()) {
        // We loaded old format, migrate to new one.
        Json::Value t_json;
        t_json["hashes"]["sha256"] = it.key();
        Uptane::Target t((*it).asString(), t_json);
        new_versions.push_back(t);
        if (current_version != nullptr) {
          *current_version = k;
        }
      } else {
        if (current_version != nullptr && (*it)["is_current"].asBool()) {
          *current_version = k;
        }
        Uptane::Target t(it.key().asString(), *it);
        new_versions.push_back(t);
      }
    }
    *installed_versions = new_versions;
  } catch (const std::exception& ex) {
    LOG_ERROR << "Unable to parse installed_versions: " << ex.what();
    return false;
  }

  return true;
}
