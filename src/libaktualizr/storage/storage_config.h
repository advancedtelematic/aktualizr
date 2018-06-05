#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <memory>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include "utilities/config_utils.h"

enum StorageType { kFileSystem = 0, kSqlite };

struct StorageConfig {
  StorageType type{kFileSystem};
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

#endif  // STORAGE_CONFIG_H
