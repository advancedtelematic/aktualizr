#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <memory>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <libaktualizr/config_utils.h>

enum class StorageType { kFileSystem = 0, kSqlite };
std::ostream& operator<<(std::ostream& os, StorageType stype);

struct StorageConfig {
  StorageType type{StorageType::kSqlite};
  boost::filesystem::path path{"/var/sota"};

  // FS storage
  BasedPath uptane_metadata_path{"metadata"};
  BasedPath uptane_private_key_path{"ecukey.der"};
  BasedPath uptane_public_key_path{"ecukey.pub"};
  BasedPath tls_cacert_path{"root.crt"};
  BasedPath tls_pkey_path{"pkey.pem"};
  BasedPath tls_clientcert_path{"client.pem"};

  // SQLite storage
  BasedPath sqldb_path{"sql.db"};  // based on `/var/sota`

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct ImportConfig {
  boost::filesystem::path base_path{"/var/sota/import"};
  BasedPath uptane_private_key_path{""};
  BasedPath uptane_public_key_path{""};
  BasedPath tls_cacert_path{""};
  BasedPath tls_pkey_path{""};
  BasedPath tls_clientcert_path{""};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

template <>
inline void CopyFromConfig(StorageType& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string storage_type{StripQuotesFromStrings(value.get())};
    if (storage_type == "sqlite") {
      dest = StorageType::kSqlite;
    } else {
      dest = StorageType::kFileSystem;
    }
  }
}

#endif  // STORAGE_CONFIG_H
