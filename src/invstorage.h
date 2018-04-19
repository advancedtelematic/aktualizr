#ifndef INVSTORAGE_H_
#define INVSTORAGE_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <memory>
#include <string>

#include "uptane/tuf.h"
#include "utilities/config_utils.h"

enum StorageType { kFileSystem = 0, kSqlite };

struct StorageConfig {
  StorageType type{kFileSystem};
  boost::filesystem::path path{"/var/sota"};
  // TODO: merge with path once SQLStorage class is complete
  boost::filesystem::path sqldb_path{"/var/sota/storage.db"};
  // FS storage
  boost::filesystem::path uptane_metadata_path{"metadata"};
  boost::filesystem::path uptane_private_key_path{"ecukey.pem"};
  boost::filesystem::path uptane_public_key_path{"ecukey.pub"};
  boost::filesystem::path tls_cacert_path{"ca.pem"};
  boost::filesystem::path tls_pkey_path{"pkey.pem"};
  boost::filesystem::path tls_clientcert_path{"client.pem"};

  // SQLite storage
  boost::filesystem::path schemas_path{"/usr/lib/sota/schemas"};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct ImportConfig {
  boost::filesystem::path uptane_private_key_path;
  boost::filesystem::path uptane_public_key_path;
  boost::filesystem::path tls_cacert_path;
  boost::filesystem::path tls_pkey_path;
  boost::filesystem::path tls_clientcert_path;

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class INvStorage;
class FSStorage;
class SQLStorage;

typedef void (INvStorage::*store_data_t)(const std::string& data);
typedef bool (INvStorage::*load_data_t)(std::string* data);

typedef std::pair<std::string, bool> InstalledVersion;

enum EcuState { kOld = 0, kNotRegistered };
struct MisconfiguredEcu {
  MisconfiguredEcu(const std::string& serial_in, const std::string hardware_id_in, EcuState state_in)
      : serial(serial_in), hardware_id(hardware_id_in), state(state_in) {}
  std::string serial;
  std::string hardware_id;
  EcuState state;
};

class StorageTargetWHandle {
 public:
  class WriteError : public std::runtime_error {
   public:
    WriteError(const std::string& what) : std::runtime_error(what) {}
  };
  virtual ~StorageTargetWHandle() = default;
  virtual size_t wfeed(const uint8_t* buf, size_t size) = 0;
  virtual void wcommit() = 0;
  virtual void wabort() = 0;

  friend std::istream& operator>>(std::istream& is, StorageTargetWHandle& handle) {
    std::array<uint8_t, 256> arr;
    while (!is.eof()) {
      is.read(reinterpret_cast<char*>(arr.data()), arr.size());

      handle.wfeed(arr.data(), is.gcount());
    }
    handle.wcommit();

    return is;
  }
};

class StorageTargetRHandle {
 public:
  class ReadError : public std::runtime_error {
   public:
    ReadError(const std::string& what) : std::runtime_error(what) {}
  };
  virtual ~StorageTargetRHandle() = default;
  virtual size_t rsize() const = 0;
  virtual size_t rread(uint8_t* buf, size_t size) = 0;
  virtual void rclose() = 0;

  friend std::ostream& operator<<(std::ostream& os, StorageTargetRHandle& handle) {
    std::array<uint8_t, 256> arr;
    size_t written = 0;
    while (written < handle.rsize()) {
      size_t nread = handle.rread(arr.data(), arr.size());

      os.write(reinterpret_cast<char*>(arr.data()), nread);
      written += nread;
    }

    return os;
  }
};

// Functions loading/storing multiple pieces of data are supposed to do so atomically as far as implementation makes it
// possible
class INvStorage {
 public:
  INvStorage(const StorageConfig& config) : config_(config) {}
  virtual ~INvStorage() {}
  virtual void storePrimaryKeys(const std::string& public_key, const std::string& private_key) = 0;
  virtual bool loadPrimaryKeys(std::string* public_key, std::string* private_key) = 0;
  virtual bool loadPrimaryPublic(std::string* public_key) = 0;
  virtual bool loadPrimaryPrivate(std::string* private_key) = 0;
  virtual void clearPrimaryKeys() = 0;

  virtual void storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) = 0;
  virtual void storeTlsCa(const std::string& ca) = 0;
  virtual void storeTlsCert(const std::string& cert) = 0;
  virtual void storeTlsPkey(const std::string& pkey) = 0;
  virtual bool loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) = 0;
  virtual bool loadTlsCa(std::string* ca) = 0;
  virtual bool loadTlsCert(std::string* cert) = 0;
  virtual bool loadTlsPkey(std::string* cert) = 0;
  virtual void clearTlsCreds() = 0;

  virtual void storeMetadata(const Uptane::MetaPack& metadata) = 0;
  virtual bool loadMetadata(Uptane::MetaPack* metadata) = 0;
  virtual void clearMetadata() = 0;

  virtual void storeDeviceId(const std::string& device_id) = 0;
  virtual bool loadDeviceId(std::string* device_id) = 0;
  virtual void clearDeviceId() = 0;

  virtual void storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) = 0;
  virtual bool loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) = 0;
  virtual void clearEcuSerials() = 0;

  virtual void storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) = 0;
  virtual bool loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) = 0;
  virtual void clearMisconfiguredEcus() = 0;

  virtual void storeEcuRegistered() = 0;
  virtual bool loadEcuRegistered() = 0;
  virtual void clearEcuRegistered() = 0;

  virtual void storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                                      const std::string& current_hash) = 0;
  virtual std::string loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) = 0;
  virtual void clearInstalledVersions() = 0;

  // Incremental file API
  virtual std::unique_ptr<StorageTargetWHandle> allocateTargetFile(bool from_director, const std::string& filename,
                                                                   size_t size) = 0;
  virtual std::unique_ptr<StorageTargetRHandle> openTargetFile(const std::string& filename) = 0;
  virtual void removeTargetFile(const std::string& filename) = 0;

  virtual void cleanUp() = 0;

  // Not purely virtual
  virtual void importData(const ImportConfig& import_config);
  void saveInstalledVersion(const Uptane::Target& target);

  static std::shared_ptr<INvStorage> newStorage(const StorageConfig& config,
                                                const boost::filesystem::path& path = "/var/sota");
  static void FSSToSQLS(const std::shared_ptr<INvStorage>& fs_storage, std::shared_ptr<INvStorage>& sql_storage);
  virtual StorageType type() = 0;

 private:
  void importSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importUpdateSimple(store_data_t store_func, load_data_t load_func, boost::filesystem::path imported_data_path);
  void importPrimaryKeys(const boost::filesystem::path& import_pubkey_path,
                         const boost::filesystem::path& import_privkey_path);

 protected:
  const StorageConfig& config_;
};

#endif  // INVSTORAGE_H_
