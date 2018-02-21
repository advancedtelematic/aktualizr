#include "fsstorage.h"

#include <iostream>

#include <boost/scoped_array.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "logging.h"
#include "utils.h"

FSStorage::FSStorage(const StorageConfig& config) : INvStorage(config) {
  boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo");
  boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director");
  boost::filesystem::create_directories(config_.path / "targets");
}

FSStorage::~FSStorage() {
  // TODO: clear director_files, image_files
}

void FSStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  storePrimaryPublic(public_key);
  storePrimaryPrivate(private_key);
}

void FSStorage::storePrimaryPublic(const std::string& public_key) {
  boost::filesystem::path public_key_path = Utils::absolutePath(config_.path, config_.uptane_public_key_path);
  boost::filesystem::remove(public_key_path);
  Utils::writeFile(public_key_path, public_key);

  sync();
}

void FSStorage::storePrimaryPrivate(const std::string& private_key) {
  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  boost::filesystem::remove(private_key_path);
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

  if (public_key) *public_key = Utils::readFile(public_key_path.string());
  return true;
}

bool FSStorage::loadPrimaryPrivate(std::string* private_key) {
  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  if (!boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (private_key) *private_key = Utils::readFile(private_key_path.string());
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
  boost::filesystem::remove(ca_path);
  Utils::writeFile(ca_path, ca);
  sync();
}

void FSStorage::storeTlsCert(const std::string& cert) {
  boost::filesystem::path cert_path(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::remove(cert_path);
  Utils::writeFile(cert_path, cert);
  sync();
}

void FSStorage::storeTlsPkey(const std::string& pkey) {
  boost::filesystem::path pkey_path(Utils::absolutePath(config_.path, config_.tls_pkey_path));
  boost::filesystem::remove(pkey_path);
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
  if (ca) {
    *ca = Utils::readFile(ca_path.string());
  }
  if (cert) {
    *cert = Utils::readFile(cert_path.string());
  }
  if (pkey) {
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
  if (!boost::filesystem::exists(path)) return false;

  if (data) *data = Utils::readFile(path.string());

  return true;
}

bool FSStorage::loadTlsCa(std::string* ca) { return loadTlsCommon(ca, config_.tls_cacert_path); }

bool FSStorage::loadTlsCert(std::string* cert) { return loadTlsCommon(cert, config_.tls_clientcert_path); }

bool FSStorage::loadTlsPkey(std::string* pkey) { return loadTlsCommon(pkey, config_.tls_pkey_path); }

void FSStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

  Utils::writeFile((director_path / "root.json"), metadata.director_root.toJson());
  Utils::writeFile((director_path / "targets.json"), metadata.director_targets.toJson());
  Utils::writeFile((image_path / "root.json"), metadata.image_root.toJson());
  Utils::writeFile((image_path / "targets.json"), metadata.image_targets.toJson());
  Utils::writeFile((image_path / "timestamp.json"), metadata.image_timestamp.toJson());
  Utils::writeFile((image_path / "snapshot.json"), metadata.image_snapshot.toJson());
  sync();
}

bool FSStorage::loadMetadata(Uptane::MetaPack* metadata) {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

  if (!boost::filesystem::exists(director_path / "root.json") ||
      !boost::filesystem::exists(director_path / "targets.json") ||
      !boost::filesystem::exists(image_path / "root.json") || !boost::filesystem::exists(image_path / "targets.json") ||
      !boost::filesystem::exists(image_path / "timestamp.json") ||
      !boost::filesystem::exists(image_path / "snapshot.json"))
    return false;

  Json::Value json = Utils::parseJSONFile(director_path / "root.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->director_root = Uptane::Root("director", json);

  json = Utils::parseJSONFile(director_path / "targets.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->director_targets = Uptane::Targets(json);

  json = Utils::parseJSONFile(image_path / "root.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_root = Uptane::Root("image", json);

  json = Utils::parseJSONFile(image_path / "targets.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_targets = Uptane::Targets(json);

  json = Utils::parseJSONFile(image_path / "timestamp.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_timestamp = Uptane::TimestampMeta(json);

  json = Utils::parseJSONFile(image_path / "snapshot.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_snapshot = Uptane::Snapshot(json);

  return true;
}

void FSStorage::clearMetadata() {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

  boost::filesystem::remove(director_path / "root.json");
  boost::filesystem::remove(director_path / "targets.json");
  boost::filesystem::remove(director_path / "root.json");
  boost::filesystem::remove(director_path / "targets.json");
  boost::filesystem::remove(image_path / "timestamp.json");
  boost::filesystem::remove(image_path / "snapshot.json");
}

void FSStorage::storeDeviceId(const std::string& device_id) {
  Utils::writeFile(Utils::absolutePath(config_.path, "device_id"), device_id);
}

bool FSStorage::loadDeviceId(std::string* device_id) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "device_id").string())) return false;

  if (device_id) *device_id = Utils::readFile(Utils::absolutePath(config_.path, "device_id").string());
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

void FSStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_serial"), serials[0].first);
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"), serials[0].second);

    boost::filesystem::remove_all(Utils::absolutePath(config_.path, "secondaries_list"));
    std::vector<std::pair<std::string, std::string> >::const_iterator it;
    std::ofstream file(Utils::absolutePath(config_.path, "secondaries_list").c_str());
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      // Assuming that there are no tabs and linebreaks in serials and hardware ids
      file << it->first << "\t" << it->second << "\n";
    }
    file.close();
  }
}

bool FSStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
  std::string buf;
  std::string serial;
  std::string hw_id;
  if (!boost::filesystem::exists((Utils::absolutePath(config_.path, "primary_ecu_serial")))) {
    return false;
  }
  serial = Utils::readFile(Utils::absolutePath(config_.path, "primary_ecu_serial").string());
  // use default hardware ID for backwards compatibility
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"))) {
    hw_id = Utils::getHostname();
  } else {
    hw_id = Utils::readFile(Utils::absolutePath(config_.path, "primary_ecu_hardware_id").string());
  }

  if (serials) {
    serials->push_back(std::pair<std::string, std::string>(serial, hw_id));
  }

  // return true for backwards compatibility
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "secondaries_list"))) {
    return true;
  }
  std::ifstream file(Utils::absolutePath(config_.path, "secondaries_list").c_str());
  while (std::getline(file, buf)) {
    size_t tab = buf.find('\t');
    serial = buf.substr(0, tab);
    try {
      hw_id = buf.substr(tab + 1);
    } catch (const std::out_of_range& e) {
      if (serials) {
        serials->clear();
      }
      file.close();
      return false;
    }
    if (serials) {
      serials->push_back(std::pair<std::string, std::string>(serial, hw_id));
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
    ecu["serial"] = it->serial;
    ecu["hardware_id"] = it->hardware_id;
    ecu["state"] = it->state;
    json.append(ecu);
  }
  Utils::writeFile(Utils::absolutePath(config_.path, "misconfigured_ecus"), Json::FastWriter().write(json));
}

bool FSStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "misconfigured_ecus"))) return false;
  Json::Value content_json = Utils::parseJSONFile(Utils::absolutePath(config_.path, "misconfigured_ecus").string());

  for (Json::ValueIterator it = content_json.begin(); it != content_json.end(); ++it) {
    ecus->push_back(MisconfiguredEcu((*it)["serial"].asString(), (*it)["hardware_id"].asString(),
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
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) return current_hash;
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

class FSTargetWHandle : public StorageTargetWHandle {
 public:
  FSTargetWHandle(const FSStorage& storage, const std::string& filename, size_t size)
      : storage_(storage), filename_(filename), expected_size_(size), written_size_(0), closed_(false), fp_(0) {
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
      this->wabort();
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
    struct stat sb;
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
