#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <memory>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include "utilities/config_utils.h"

enum class StorageType { FileSystem = 0, Sqlite };

struct StorageConfig {
  StorageType type{StorageType::FileSystem};
  boost::filesystem::path path{"/var/sota"};

  // FS storage
  boost::filesystem::path uptane_metadata_path{"metadata"};
  boost::filesystem::path uptane_private_key_path{"ecukey.der"};
  boost::filesystem::path uptane_public_key_path{"ecukey.pub"};
  boost::filesystem::path tls_cacert_path{"root.crt"};
  boost::filesystem::path tls_pkey_path{"pkey.pem"};
  boost::filesystem::path tls_clientcert_path{"client.pem"};

  // SQLite storage
  boost::filesystem::path sqldb_path{"/var/sota/sql.db"};

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

template <>
inline void CopyFromConfig(StorageType& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string storage_type{StripQuotesFromStrings(value.get())};
    if (storage_type == "sqlite") {
      dest = StorageType::Sqlite;
    } else {
      dest = StorageType::FileSystem;
    }
  }
}

#endif  // STORAGE_CONFIG_H
