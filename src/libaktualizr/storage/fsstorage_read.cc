#include "fsstorage_read.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <utility>

#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include "json/json.h"

#include <libaktualizr/utils.h>
#include "logging/logging.h"

FSStorageRead::FSStorageRead(const StorageConfig& config) : config_(config) {
  boost::filesystem::path image_path = config_.uptane_metadata_path.get(config_.path) / "repo";
  boost::filesystem::path director_path = config_.uptane_metadata_path.get(config_.path) / "director";
  // migrate from old unversioned Uptane Root metadata
  {
    for (auto repo : {Uptane::RepositoryType::Director(), Uptane::RepositoryType::Image()}) {
      boost::filesystem::path& meta_dir = repo == (Uptane::RepositoryType::Director()) ? director_path : image_path;
      boost::filesystem::path meta_path = meta_dir / Uptane::Version().RoleFileName(Uptane::Role::Root());
      if (boost::filesystem::exists(meta_path)) {
        std::string data = Utils::readFile(meta_path);
        Uptane::Version version{Uptane::extractVersionUntrusted(data)};
        boost::filesystem::remove(meta_path);
        if (version.version() >= 0) {
          Utils::writeFile(meta_dir / version.RoleFileName(Uptane::Role::Root()), data);
        }
      }
    }
  }

  latest_director_root = findMaxVersion(director_path, Uptane::Role::Root());
  latest_image_root = findMaxVersion(image_path, Uptane::Role::Root());
}

bool FSStorageRead::loadPrimaryKeys(std::string* public_key, std::string* private_key) const {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool FSStorageRead::loadPrimaryPublic(std::string* public_key) const {
  boost::filesystem::path public_key_path = config_.uptane_public_key_path.get(config_.path);
  if (!boost::filesystem::exists(public_key_path)) {
    return false;
  }

  if (public_key != nullptr) {
    *public_key = Utils::readFile(public_key_path.string());
  }
  return true;
}

bool FSStorageRead::loadPrimaryPrivate(std::string* private_key) const {
  boost::filesystem::path private_key_path = config_.uptane_private_key_path.get(config_.path);
  if (!boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (private_key != nullptr) {
    *private_key = Utils::readFile(private_key_path.string());
  }
  return true;
}

bool FSStorageRead::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) const {
  boost::filesystem::path ca_path(config_.tls_cacert_path.get(config_.path));
  boost::filesystem::path cert_path(config_.tls_clientcert_path.get(config_.path));
  boost::filesystem::path pkey_path(config_.tls_pkey_path.get(config_.path));
  if (!boost::filesystem::exists(ca_path) || boost::filesystem::is_directory(ca_path) ||
      !boost::filesystem::exists(cert_path) || boost::filesystem::is_directory(cert_path) ||
      !boost::filesystem::exists(pkey_path) || boost::filesystem::is_directory(pkey_path)) {
    return false;
  }
  if (ca != nullptr) {
    *ca = Utils::readFile(ca_path.string());
  }
  if (cert != nullptr) {
    *cert = Utils::readFile(cert_path.string());
  }
  if (pkey != nullptr) {
    *pkey = Utils::readFile(pkey_path.string());
  }
  return true;
}

bool FSStorageRead::loadTlsCommon(std::string* data, const BasedPath& path_in) const {
  boost::filesystem::path path(path_in.get(config_.path));
  if (!boost::filesystem::exists(path)) {
    return false;
  }

  if (data != nullptr) {
    *data = Utils::readFile(path.string());
  }

  return true;
}

bool FSStorageRead::loadTlsCa(std::string* ca) const { return loadTlsCommon(ca, config_.tls_cacert_path); }

bool FSStorageRead::loadTlsCert(std::string* cert) const { return loadTlsCommon(cert, config_.tls_clientcert_path); }

bool FSStorageRead::loadTlsPkey(std::string* pkey) const { return loadTlsCommon(pkey, config_.tls_pkey_path); }

bool FSStorageRead::loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) const {
  boost::filesystem::path metafile;
  switch (repo) {
    case (Uptane::RepositoryType::Director()):
      if (version.version() < 0) {
        version = latest_director_root;
      }
      metafile =
          config_.uptane_metadata_path.get(config_.path) / "director" / version.RoleFileName(Uptane::Role::Root());
      break;

    case (Uptane::RepositoryType::Image()):
      if (version.version() < 0) {
        version = latest_director_root;
      }
      metafile = config_.uptane_metadata_path.get(config_.path) / "repo" / version.RoleFileName(Uptane::Role::Root());
      break;

    default:
      return false;
  }

  if (version.version() < 0) {
    return false;
  }

  if (!boost::filesystem::exists(metafile)) {
    return false;
  }

  if (data != nullptr) {
    *data = Utils::readFile(metafile);
  }
  return true;
}

bool FSStorageRead::loadNonRoot(std::string* data, Uptane::RepositoryType repo, const Uptane::Role& role) const {
  boost::filesystem::path metafile;
  switch (repo) {
    case (Uptane::RepositoryType::Director()):
      metafile = config_.uptane_metadata_path.get(config_.path) / "director" / Uptane::Version().RoleFileName(role);
      break;

    case (Uptane::RepositoryType::Image()):
      metafile = config_.uptane_metadata_path.get(config_.path) / "repo" / Uptane::Version().RoleFileName(role);
      break;

    default:
      return false;
  }

  if (!boost::filesystem::exists(metafile)) {
    return false;
  }

  if (data != nullptr) {
    *data = Utils::readFile(metafile);
  }
  return true;
}

bool FSStorageRead::loadDeviceId(std::string* device_id) const {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "device_id").string())) {
    return false;
  }

  if (device_id != nullptr) {
    *device_id = Utils::readFile(Utils::absolutePath(config_.path, "device_id").string());
  }
  return true;
}

bool FSStorageRead::loadEcuRegistered() const {
  return boost::filesystem::exists(Utils::absolutePath(config_.path, "is_registered").string());
}

bool FSStorageRead::loadEcuSerials(EcuSerials* serials) const {
  std::string buf;
  std::string serial;
  std::string hw_id;

  const boost::filesystem::path serial_path = Utils::absolutePath(config_.path, "primary_ecu_serial");
  if (!boost::filesystem::exists(serial_path)) {
    return false;
  }
  serial = Utils::readFile(serial_path.string());
  // use default hardware ID for backwards compatibility
  const boost::filesystem::path hw_id_path = Utils::absolutePath(config_.path, "primary_ecu_hardware_id");
  if (!boost::filesystem::exists(hw_id_path)) {
    hw_id = Utils::getHostname();
  } else {
    hw_id = Utils::readFile(hw_id_path.string());
  }

  if (serials != nullptr) {
    serials->push_back({Uptane::EcuSerial(serial), Uptane::HardwareIdentifier(hw_id)});
  }

  // return true for backwards compatibility
  const boost::filesystem::path sec_list_path = Utils::absolutePath(config_.path, "secondaries_list");
  if (!boost::filesystem::exists(sec_list_path)) {
    return true;
  }
  std::ifstream file(sec_list_path.c_str());
  while (std::getline(file, buf)) {
    size_t tab = buf.find('\t');
    serial = buf.substr(0, tab);
    try {
      hw_id = buf.substr(tab + 1);
    } catch (const std::out_of_range& e) {
      if (serials != nullptr) {
        serials->clear();
      }
      file.close();
      return false;
    }
    if (serials != nullptr) {
      serials->push_back({Uptane::EcuSerial(serial), Uptane::HardwareIdentifier(hw_id)});
    }
  }
  file.close();
  return true;
}

bool FSStorageRead::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) const {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "misconfigured_ecus"))) {
    return false;
  }
  Json::Value content_json = Utils::parseJSONFile(Utils::absolutePath(config_.path, "misconfigured_ecus").string());

  for (auto it = content_json.begin(); it != content_json.end(); ++it) {
    ecus->push_back(MisconfiguredEcu(Uptane::EcuSerial((*it)["serial"].asString()),
                                     Uptane::HardwareIdentifier((*it)["hardware_id"].asString()),
                                     static_cast<EcuState>((*it)["state"].asInt())));
  }
  return true;
}

bool FSStorageRead::loadInstalledVersions(std::vector<Uptane::Target>* installed_versions,
                                          size_t* current_version) const {
  const boost::filesystem::path path = Utils::absolutePath(config_.path, "installed_versions");
  return INvStorage::fsReadInstalledVersions(path, installed_versions, current_version);
}

bool FSStorageRead::splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version) {
  size_t dot_pos = full_name.find('.');

  // doesn't have a dot
  if (dot_pos == std::string::npos) {
    return false;
  }
  std::string prefix = full_name.substr(0, dot_pos);
  if (role_name != nullptr) {
    *role_name = full_name.substr(dot_pos + 1);
  }

  try {
    auto v = boost::lexical_cast<int>(prefix);
    if (version != nullptr) {
      *version = v;
    }
  } catch (const boost::bad_lexical_cast&) {
    return false;
  }
  return true;
}

Uptane::Version FSStorageRead::findMaxVersion(const boost::filesystem::path& meta_directory, const Uptane::Role& role) {
  int version = -1;
  if (!boost::filesystem::exists(meta_directory)) {
    return {};
  }

  boost::filesystem::directory_iterator it{meta_directory};
  for (; it != boost::filesystem::directory_iterator(); ++it) {
    if (!boost::filesystem::is_regular_file(it->path())) {
      continue;
    }
    std::string name = it->path().filename().native();
    int file_version;
    std::string file_role;
    if (splitNameRoleVersion(name, &file_role, &file_version)) {
      if (file_role == Uptane::Version().RoleFileName(role) && file_version > version) {
        version = file_version;
      }
    }
  }

  return Uptane::Version(version);
}

// clear methods, to clean the storage files after a migration

void FSStorageRead::clearPrimaryKeys() {
  boost::filesystem::remove(config_.uptane_public_key_path.get(config_.path));
  boost::filesystem::remove(config_.uptane_private_key_path.get(config_.path));
}

void FSStorageRead::clearTlsCreds() {
  boost::filesystem::remove(config_.tls_cacert_path.get(config_.path));
  boost::filesystem::remove(config_.tls_clientcert_path.get(config_.path));
  boost::filesystem::remove(config_.tls_pkey_path.get(config_.path));
}

void FSStorageRead::clearNonRootMeta(Uptane::RepositoryType repo) {
  boost::filesystem::path meta_path;
  switch (repo) {
    case Uptane::RepositoryType::Image():
      meta_path = config_.uptane_metadata_path.get(config_.path) / "repo";
      break;
    case Uptane::RepositoryType::Director():
      meta_path = config_.uptane_metadata_path.get(config_.path) / "director";
      break;
    default:
      return;
  }

  boost::filesystem::directory_iterator it{meta_path};
  for (; it != boost::filesystem::directory_iterator(); ++it) {
    for (const auto& role : Uptane::Role::Roles()) {
      if (role == Uptane::Role::Root()) {
        continue;
      }
      std::string role_name;
      std::string fn = it->path().filename().native();
      if (fn == Uptane::Version().RoleFileName(role) ||
          (splitNameRoleVersion(fn, &role_name, nullptr) && (role_name == Uptane::Version().RoleFileName(role)))) {
        boost::filesystem::remove(it->path());
      }
    }
  }
}

void FSStorageRead::clearMetadata() {
  for (const auto& meta_path : {config_.uptane_metadata_path.get(config_.path) / "repo",
                                config_.uptane_metadata_path.get(config_.path) / "director"}) {
    if (!boost::filesystem::exists(meta_path)) {
      return;
    }

    boost::filesystem::directory_iterator it{meta_path};
    for (; it != boost::filesystem::directory_iterator(); ++it) {
      boost::filesystem::remove(it->path());
    }
  }
}

void FSStorageRead::clearDeviceId() { boost::filesystem::remove(Utils::absolutePath(config_.path, "device_id")); }

void FSStorageRead::clearEcuRegistered() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "is_registered"));
}

void FSStorageRead::clearEcuSerials() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_serial"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "secondaries_list"));
}

void FSStorageRead::clearMisconfiguredEcus() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "misconfigured_ecus"));
}

void FSStorageRead::clearInstalledVersions() {
  if (boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) {
    boost::filesystem::remove(Utils::absolutePath(config_.path, "installed_versions"));
  }
}

void FSStorageRead::clearInstallationResult() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "installation_result"));
}

void FSStorageRead::cleanUpAll() {
  clearPrimaryKeys();
  clearTlsCreds();
  clearDeviceId();
  clearEcuSerials();
  clearEcuRegistered();
  clearMisconfiguredEcus();
  clearInstalledVersions();
  clearInstallationResult();
  clearMetadata();

  boost::filesystem::remove_all(config_.uptane_metadata_path.get(config_.path));
  boost::filesystem::remove_all(config_.path / "targets");
}

bool FSStorageRead::FSStoragePresent(const StorageConfig& config) {
  return boost::filesystem::exists(Utils::absolutePath(config.path, "is_registered").string());
}
