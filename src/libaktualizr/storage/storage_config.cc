#include <libaktualizr/config.h>
#include "utilities/config_utils.h"

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
