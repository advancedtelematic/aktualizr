#include "libaktualizr/config.h"

#include "logging/logging.h"
#include "utilities/config_utils.h"
#include "utilities/utils.h"

void BaseConfig::updateFromToml(const boost::filesystem::path& filename) {
  LOG_INFO << "Reading config: " << filename;
  if (!boost::filesystem::exists(filename)) {
    throw std::runtime_error("Config file " + filename.string() + " does not exist.");
  }
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename.string(), pt);
  updateFromPropertyTree(pt);
}

void BaseConfig::updateFromDirs(const std::vector<boost::filesystem::path>& configs) {
  std::map<std::string, boost::filesystem::path> configs_map;
  for (const auto& config : configs) {
    if (!boost::filesystem::exists(config)) {
      continue;
    }
    if (boost::filesystem::is_directory(config)) {
      for (const auto& config_file : Utils::getDirEntriesByExt(config, ".toml")) {
        configs_map[config_file.filename().string()] = config_file;
      }
    } else {
      configs_map[config.filename().string()] = config;
    }
  }
  for (const auto& config_file : configs_map) {
    updateFromToml(config_file.second);
  }
}

void P11Config::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(module, "module", pt);
  CopyFromConfig(pass, "pass", pt);
  CopyFromConfig(uptane_key_id, "uptane_key_id", pt);
  CopyFromConfig(tls_cacert_id, "tls_cacert_id", pt);
  CopyFromConfig(tls_pkey_id, "tls_pkey_id", pt);
  CopyFromConfig(tls_clientcert_id, "tls_clientcert_id", pt);
}

void P11Config::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, module, "module");
  writeOption(out_stream, pass, "pass");
  writeOption(out_stream, uptane_key_id, "uptane_key_id");
  writeOption(out_stream, tls_cacert_id, "tls_ca_id");
  writeOption(out_stream, tls_pkey_id, "tls_pkey_id");
  writeOption(out_stream, tls_clientcert_id, "tls_clientcert_id");
}
