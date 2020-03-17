#ifndef CONFIG_UTILS_H_
#define CONFIG_UTILS_H_

#include <string>

#include <boost/property_tree/ini_parser.hpp>

#include "logging/logging.h"
#include "types.h"
#include "utils.h"

/*
 The following uses a small amount of template hackery to provide a nice
 interface to load the sota.toml config file. StripQuotesFromStrings is
 templated, and passes everything that isn't a string straight through.
 Strings in toml are always double-quoted, and we remove them by specializing
 StripQuotesFromStrings for std::string.

 The end result is that the sequence of calls in Config::updateFromToml are
 pretty much a direct expression of the required behaviour: load this variable
 from this config entry, and print a warning at the

 Note that default values are defined by Config's default constructor.
 */

template <typename T>
inline T StripQuotesFromStrings(const T& value);

template <>
inline std::string StripQuotesFromStrings<std::string>(const std::string& value) {
  return Utils::stripQuotes(value);
}

template <typename T>
inline T StripQuotesFromStrings(const T& value) {
  return value;
}

template <typename T>
inline T addQuotesToStrings(const T& value);

template <>
inline std::string addQuotesToStrings<std::string>(const std::string& value) {
  return Utils::addQuotes(value);
}

template <typename T>
inline T addQuotesToStrings(const T& value) {
  return value;
}

template <typename T>
inline void writeOption(std::ostream& sink, const T& data, const std::string& option_name) {
  sink << option_name << " = " << addQuotesToStrings(data) << "\n";
}

template <typename T>
inline void CopyFromConfig(T& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<T> value = pt.get_optional<T>(option_name);
  if (value.is_initialized()) {
    dest = StripQuotesFromStrings(value.get());
  }
}

template <>
inline void CopyFromConfig(KeyType& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string key_type{StripQuotesFromStrings(value.get())};
    if (key_type == "RSA2048") {
      dest = KeyType::kRSA2048;
    } else if (key_type == "RSA3072") {
      dest = KeyType::kRSA3072;
    } else if (key_type == "RSA4096") {
      dest = KeyType::kRSA4096;
    } else if (key_type == "ED25519") {
      dest = KeyType::kED25519;
    } else {
      dest = KeyType::kUnknown;
    }
  }
}

template <>
inline void CopyFromConfig(CryptoSource& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string crypto_source{StripQuotesFromStrings(value.get())};
    if (crypto_source == "pkcs11") {
      dest = CryptoSource::kPkcs11;
    } else {
      dest = CryptoSource::kFile;
    }
  }
}

template <>
inline void CopyFromConfig(BasedPath& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    BasedPath bp{StripQuotesFromStrings(value.get())};
    dest = bp;
  }
}

template <typename T>
inline void CopySubtreeFromConfig(T& dest, const std::string& subtree_name, const boost::property_tree::ptree& pt) {
  auto subtree = pt.get_child_optional(subtree_name);
  if (subtree.is_initialized()) {
    dest.updateFromPropertyTree(subtree.get());
  } else {
    // call with empty tree so that default value warnings are preserved
    dest.updateFromPropertyTree(boost::property_tree::ptree());
  }
}

template <typename T>
inline void WriteSectionToStream(T& sec, const std::string& section_name, std::ostream& os) {
  os << std::boolalpha;
  os << "[" << section_name << "]\n";
  sec.writeToStream(os);
  os << "\n";
}

class BaseConfig {
 public:
  virtual ~BaseConfig() = default;
  void updateFromToml(const boost::filesystem::path& filename) {
    LOG_INFO << "Reading config: " << filename;
    if (!boost::filesystem::exists(filename)) {
      throw std::runtime_error("Config file " + filename.string() + " does not exist.");
    }
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(filename.string(), pt);
    updateFromPropertyTree(pt);
  }
  virtual void updateFromPropertyTree(const boost::property_tree::ptree& pt) = 0;

 protected:
  void updateFromDirs(const std::vector<boost::filesystem::path>& configs) {
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

  static void checkDirs(const std::vector<boost::filesystem::path>& configs) {
    for (const auto& config : configs) {
      if (!boost::filesystem::exists(config)) {
        throw std::runtime_error("Config directory " + config.string() + " does not exist.");
      }
    }
  }

  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
};

#endif  // CONFIG_UTILS_H_
