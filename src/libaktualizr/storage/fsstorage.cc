#include "fsstorage.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <utility>

#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include "json/json.h"

#include "logging/logging.h"
#include "utilities/utils.h"

bool FSStorage::splitNameRoleVersion(const std::string& full_name, std::string* role_name, int* version) {
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

Uptane::Version FSStorage::findMaxVersion(const boost::filesystem::path& meta_directory, Uptane::Role role) {
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

FSStorage::FSStorage(const StorageConfig& config, bool migration_only) : INvStorage(config) {
  struct stat st {};

  if (!migration_only) {
    stat(config_.path.c_str(), &st);
    if ((st.st_mode & (S_IWOTH | S_IWGRP)) != 0) {
      throw std::runtime_error("Storage directory has unsafe permissions");
    }
    if ((st.st_mode & (S_IRGRP | S_IROTH)) != 0) {
      // Remove read permissions for group and others
      chmod(config_.path.c_str(), S_IRWXU);
    }
    try {
      boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo");
      boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) /
                                            "director");
      boost::filesystem::create_directories(config_.path / "targets");
    } catch (const boost::filesystem::filesystem_error& e) {
      LOG_ERROR << "FSStorage failed to create directories:" << e.what();
      throw;
    }
  }

  boost::filesystem::path images_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";
  // migrate from old unversioned Uptane root meta
  {
    for (auto repo : {Uptane::RepositoryType::Director, Uptane::RepositoryType::Images}) {
      boost::filesystem::path& meta_dir = repo == (Uptane::RepositoryType::Director) ? director_path : images_path;
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
  latest_images_root = findMaxVersion(images_path, Uptane::Role::Root());
}

void FSStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  boost::filesystem::path public_key_path = Utils::absolutePath(config_.path, config_.uptane_public_key_path);
  Utils::writeFile(public_key_path, public_key);

  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  Utils::writeFile(private_key_path, private_key);

  sync();
}

bool FSStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool FSStorage::loadPrimaryPublic(std::string* public_key) {
  boost::filesystem::path public_key_path = Utils::absolutePath(config_.path, config_.uptane_public_key_path);
  if (!boost::filesystem::exists(public_key_path)) {
    return false;
  }

  if (public_key != nullptr) {
    *public_key = Utils::readFile(public_key_path.string());
  }
  return true;
}

bool FSStorage::loadPrimaryPrivate(std::string* private_key) {
  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  if (!boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (private_key != nullptr) {
    *private_key = Utils::readFile(private_key_path.string());
  }
  return true;
}

void FSStorage::clearPrimaryKeys() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.uptane_public_key_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.uptane_private_key_path));
}

void FSStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  storeTlsCa(ca);
  storeTlsCert(cert);
  storeTlsPkey(pkey);
}

void FSStorage::storeTlsCa(const std::string& ca) {
  boost::filesystem::path ca_path(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  Utils::writeFile(ca_path, ca);
  sync();
}

void FSStorage::storeTlsCert(const std::string& cert) {
  boost::filesystem::path cert_path(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  Utils::writeFile(cert_path, cert);
  sync();
}

void FSStorage::storeTlsPkey(const std::string& pkey) {
  boost::filesystem::path pkey_path(Utils::absolutePath(config_.path, config_.tls_pkey_path));
  Utils::writeFile(pkey_path, pkey);
  sync();
}

bool FSStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  boost::filesystem::path cert_path(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::path pkey_path(Utils::absolutePath(config_.path, config_.tls_pkey_path));
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

void FSStorage::clearTlsCreds() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_pkey_path));
}

bool FSStorage::loadTlsCommon(std::string* data, const boost::filesystem::path& path_in) {
  boost::filesystem::path path(Utils::absolutePath(config_.path, path_in));
  if (!boost::filesystem::exists(path)) {
    return false;
  }

  if (data != nullptr) {
    *data = Utils::readFile(path.string());
  }

  return true;
}

bool FSStorage::loadTlsCa(std::string* ca) { return loadTlsCommon(ca, config_.tls_cacert_path); }

bool FSStorage::loadTlsCert(std::string* cert) { return loadTlsCommon(cert, config_.tls_clientcert_path); }

bool FSStorage::loadTlsPkey(std::string* pkey) { return loadTlsCommon(pkey, config_.tls_pkey_path); }

void FSStorage::storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) {
  boost::filesystem::path metafile;
  Uptane::Version* latest_version;
  switch (repo) {
    case (Uptane::RepositoryType::Director):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director" /
                 version.RoleFileName(Uptane::Role::Root());
      latest_version = &latest_director_root;
      break;

    case (Uptane::RepositoryType::Images):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo" /
                 version.RoleFileName(Uptane::Role::Root());
      latest_version = &latest_images_root;
      break;

    default:
      return;
  }

  Utils::writeFile(metafile, data);
  if (version.version() > latest_version->version()) {
    *latest_version = version;
  }
}

void FSStorage::storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) {
  boost::filesystem::path metafile;
  switch (repo) {
    case (Uptane::RepositoryType::Director):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director" /
                 Uptane::Version().RoleFileName(role);
      break;

    case (Uptane::RepositoryType::Images):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo" /
                 Uptane::Version().RoleFileName(role);
      break;

    default:
      return;
  }

  Utils::writeFile(metafile, data);
}

bool FSStorage::loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) {
  boost::filesystem::path metafile;
  switch (repo) {
    case (Uptane::RepositoryType::Director):
      if (version.version() < 0) {
        version = latest_director_root;
      }
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director" /
                 version.RoleFileName(Uptane::Role::Root());
      break;

    case (Uptane::RepositoryType::Images):
      if (version.version() < 0) {
        version = latest_director_root;
      }
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo" /
                 version.RoleFileName(Uptane::Role::Root());
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

bool FSStorage::loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) {
  boost::filesystem::path metafile;
  switch (repo) {
    case (Uptane::RepositoryType::Director):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director" /
                 Uptane::Version().RoleFileName(role);
      break;

    case (Uptane::RepositoryType::Images):
      metafile = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo" /
                 Uptane::Version().RoleFileName(role);
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

void FSStorage::clearNonRootMeta(Uptane::RepositoryType repo) {
  boost::filesystem::path meta_path;
  switch (repo) {
    case Uptane::RepositoryType::Images:
      meta_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
      break;
    case Uptane::RepositoryType::Director:
      meta_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";
      break;
    default:
      return;
  }

  boost::filesystem::directory_iterator it{meta_path};
  for (; it != boost::filesystem::directory_iterator(); ++it) {
    for (auto role : Uptane::Role::Roles()) {
      if (role == Uptane::Role::Root()) {
        continue;
      }
      std::string role_name;
      if (splitNameRoleVersion(it->path().native(), &role_name, nullptr) &&
          (role_name == Uptane::Version().RoleFileName(role))) {
        boost::filesystem::remove(it->path());
      }
    }
  }
}

void FSStorage::clearMetadata() {
  for (const auto& meta_path : {Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo",
                                Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director"}) {
    if (!boost::filesystem::exists(meta_path)) {
      return;
    }

    boost::filesystem::directory_iterator it{meta_path};
    for (; it != boost::filesystem::directory_iterator(); ++it) {
      boost::filesystem::remove(it->path());
    }
  }
}

void FSStorage::storeDeviceId(const std::string& device_id) {
  Utils::writeFile(Utils::absolutePath(config_.path, "device_id"), device_id);
}

bool FSStorage::loadDeviceId(std::string* device_id) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "device_id").string())) {
    return false;
  }

  if (device_id != nullptr) {
    *device_id = Utils::readFile(Utils::absolutePath(config_.path, "device_id").string());
  }
  return true;
}

void FSStorage::clearDeviceId() { boost::filesystem::remove(Utils::absolutePath(config_.path, "device_id")); }

void FSStorage::storeEcuRegistered() {
  Utils::writeFile(Utils::absolutePath(config_.path, "is_registered"), std::string("1"));
}

bool FSStorage::loadEcuRegistered() {
  return boost::filesystem::exists(Utils::absolutePath(config_.path, "is_registered").string());
}

void FSStorage::clearEcuRegistered() { boost::filesystem::remove(Utils::absolutePath(config_.path, "is_registered")); }

void FSStorage::storeEcuSerials(const EcuSerials& serials) {
  if (serials.size() >= 1) {
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_serial"), serials[0].first.ToString());
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"), serials[0].second.ToString());

    boost::filesystem::remove_all(Utils::absolutePath(config_.path, "secondaries_list"));
    EcuSerials::const_iterator it;
    std::ofstream file(Utils::absolutePath(config_.path, "secondaries_list").c_str());
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      // Assuming that there are no tabs and linebreaks in serials and hardware ids
      file << it->first << "\t" << it->second << "\n";
    }
    file.close();
  }
}

bool FSStorage::loadEcuSerials(EcuSerials* serials) {
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

void FSStorage::clearEcuSerials() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_serial"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "secondaries_list"));
}

void FSStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  Json::Value json(Json::arrayValue);
  std::vector<MisconfiguredEcu>::const_iterator it;
  for (it = ecus.begin(); it != ecus.end(); it++) {
    Json::Value ecu;
    ecu["serial"] = it->serial.ToString();
    ecu["hardware_id"] = it->hardware_id.ToString();
    ecu["state"] = static_cast<int>(it->state);
    json.append(ecu);
  }
  Utils::writeFile(Utils::absolutePath(config_.path, "misconfigured_ecus"), Json::FastWriter().write(json));
}

bool FSStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "misconfigured_ecus"))) {
    return false;
  }
  Json::Value content_json = Utils::parseJSONFile(Utils::absolutePath(config_.path, "misconfigured_ecus").string());

  for (Json::ValueIterator it = content_json.begin(); it != content_json.end(); ++it) {
    ecus->push_back(MisconfiguredEcu(Uptane::EcuSerial((*it)["serial"].asString()),
                                     Uptane::HardwareIdentifier((*it)["hardware_id"].asString()),
                                     static_cast<EcuState>((*it)["state"].asInt())));
  }
  return true;
}

void FSStorage::clearMisconfiguredEcus() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "misconfigured_ecus"));
}

void FSStorage::storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                                       const std::string& current_hash) {
  Json::Value content;
  std::vector<Uptane::Target>::const_iterator it;
  for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
    Json::Value installed_version;
    installed_version["hashes"]["sha256"] = it->sha256Hash();
    installed_version["length"] = Json::UInt64(it->length());
    installed_version["is_current"] = (it->sha256Hash() == current_hash);
    content[it->filename()] = installed_version;
  }
  Utils::writeFile(Utils::absolutePath(config_.path, "installed_versions"), Json::FastWriter().write(content));
}

std::string FSStorage::loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) {
  std::string current_hash;
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) {
    return current_hash;
  }
  Json::Value installed_versions_json =
      Utils::parseJSONFile(Utils::absolutePath(config_.path, "installed_versions").string());
  std::vector<Uptane::Target> new_versions;
  for (Json::ValueIterator it = installed_versions_json.begin(); it != installed_versions_json.end(); ++it) {
    if (!(*it).isObject()) {
      // We loaded old format, migrate to new one.
      Json::Value t_json;
      t_json["hashes"]["sha256"] = it.key();
      Uptane::Target t((*it).asString(), t_json);
      new_versions.push_back(t);
    } else {
      if ((*it)["is_current"].asBool()) {
        current_hash = (*it)["hashes"]["sha256"].asString();
      }
      Uptane::Target t(it.key().asString(), *it);
      new_versions.push_back(t);
    }
  }
  *installed_versions = new_versions;

  return current_hash;
}

void FSStorage::clearInstalledVersions() {
  if (boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) {
    boost::filesystem::remove(Utils::absolutePath(config_.path, "installed_versions"));
  }
}

void FSStorage::storeInstallationResult(const data::OperationResult& result) {
  Utils::writeFile(Utils::absolutePath(config_.path, "installation_result"), result.toJson());
}

bool FSStorage::loadInstallationResult(data::OperationResult* result) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "installation_result").string())) {
    return false;
  }

  if (result != nullptr) {
    *result = data::OperationResult::fromJson(
        Utils::readFile(Utils::absolutePath(config_.path, "installation_result").string()));
  }
  return true;
}

void FSStorage::clearInstallationResult() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "installation_result"));
}

class FSTargetWHandle : public StorageTargetWHandle {
 public:
  FSTargetWHandle(const FSStorage& storage, std::string filename, size_t size)
      : storage_(storage),
        filename_(std::move(filename)),
        expected_size_(size),
        written_size_(0),
        closed_(false),
        fp_(0) {
    fp_ = open(storage_.targetFilepath(filename_).c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

    if (fp_ == -1) {
      throw StorageTargetWHandle::WriteError("could not create file " + storage_.targetFilepath(filename_).string() +
                                             " on disk");
    }
  }

  FSTargetWHandle(const FSTargetWHandle& other) = delete;
  FSTargetWHandle& operator=(const FSTargetWHandle& other) = delete;

  ~FSTargetWHandle() override {
    if (!closed_) {
      LOG_WARNING << "Handle for file " << filename_ << " has not been committed or aborted, forcing abort";
      FSTargetWHandle::wabort();
    }
  }

  size_t wfeed(const uint8_t* buf, size_t size) override {
    if (written_size_ + size > expected_size_) {
      return 0;
    }

    size_t written = write(fp_, buf, size);
    written_size_ += written;

    return written;
  }

  void wcommit() override {
    closed_ = true;
    if (written_size_ != expected_size_) {
      LOG_WARNING << "got " << written_size_ << "instead of " << expected_size_ << " while writing " << filename_;
      throw StorageTargetWHandle::WriteError("could not save file " + filename_ + " to disk");
    }

    if (close(fp_) == -1) {
      throw StorageTargetWHandle::WriteError("could not close file " + filename_ + " on disk");
    }
  }

  void wabort() override {
    closed_ = true;
    close(fp_);
    boost::filesystem::remove(storage_.targetFilepath(filename_));
  }

 private:
  const FSStorage& storage_;
  const std::string filename_;
  size_t expected_size_;
  size_t written_size_;
  bool closed_;
  int fp_;
};

std::unique_ptr<StorageTargetWHandle> FSStorage::allocateTargetFile(bool from_director, const std::string& filename,
                                                                    size_t size) {
  (void)from_director;

  return std::unique_ptr<StorageTargetWHandle>(new FSTargetWHandle(*this, filename, size));
}

class FSTargetRHandle : public StorageTargetRHandle {
 public:
  FSTargetRHandle(const FSStorage& storage, const std::string& filename) : storage_(storage), closed_(false), fp_(0) {
    fp_ = open(storage_.targetFilepath(filename).c_str(), 0);
    if (fp_ == -1) {
      throw StorageTargetRHandle::ReadError("Could not open " + filename);
    }
  }

  FSTargetRHandle(const FSTargetRHandle& other) = delete;
  FSTargetRHandle& operator=(const FSTargetRHandle& other) = delete;

  ~FSTargetRHandle() override {
    if (!closed_) {
      FSTargetRHandle::rclose();
    }
  }

  size_t rsize() const override {
    struct stat sb {};
    fstat(fp_, &sb);
    return sb.st_size;
  }

  size_t rread(uint8_t* buf, size_t size) override { return read(fp_, buf, size); }

  void rclose() override { close(fp_); }

 private:
  const FSStorage& storage_;
  bool closed_;
  int fp_;
};

std::unique_ptr<StorageTargetRHandle> FSStorage::openTargetFile(const std::string& filename) {
  return std::unique_ptr<StorageTargetRHandle>(new FSTargetRHandle(*this, filename));
}

void FSStorage::removeTargetFile(const std::string& filename) {
  if (!boost::filesystem::remove(targetFilepath(filename))) {
    throw std::runtime_error("Target file " + filename + " not found");
  }
}

void FSStorage::cleanUp() {
  boost::filesystem::remove_all(Utils::absolutePath(config_.path, config_.uptane_metadata_path));
}

boost::filesystem::path FSStorage::targetFilepath(const std::string& filename) const {
  return config_.path / "targets" / filename;
}
