#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>

#include "asn1-cer.h"
#include "bootstrap.h"
#include "exceptions.h"
#include "utils.h"

std::ostream& operator<<(std::ostream& os, CryptoSource cs) {
  std::string cs_str;
  switch (cs) {
    case kFile:
      cs_str = "file";
      break;
    case kPkcs11:
      cs_str = "pkcs11";
      break;
    default:
      cs_str = "unknown";
      break;
  }
  os << '"' << cs_str << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, StorageType stype) {
  std::string stype_str;
  switch (stype) {
    case kFileSystem:
      stype_str = "filesystem";
      break;
    case kSqlite:
      stype_str = "sqlite";
      break;
    default:
      stype_str = "unknown";
      break;
  }
  os << '"' << stype_str << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, PackageManager pm) {
  std::string pm_str;
  switch (pm) {
    case kOstree:
      pm_str = "ostree";
      break;
    case kDebian:
      pm_str = "debian";
      break;
    case kNone:
    default:
      pm_str = "none";
      break;
  }
  os << '"' << pm_str << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, KeyType kt) {
  std::string kt_str;
  switch (kt) {
    case kRSA2048:
      kt_str = "RSA2048";
      break;
    case kRSA4096:
      kt_str = "RSA4096";
      break;
    case kED25519:
      kt_str = "ED25519";
      break;
    default:
      kt_str = "unknown";
      break;
  }
  os << '"' << kt_str << '"';
  return os;
}

/**
 * \par Description:
 *    Overload the << operator for the configuration class allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const Config& cfg) {
  return os << "\tPolling enabled : " << cfg.uptane.polling << std::endl
            << "\tPolling interval : " << cfg.uptane.polling_sec << std::endl;
}

// Strip leading and trailing quotes
std::string Config::stripQuotes(const std::string& value) {
  std::string res = value;
  res.erase(std::remove(res.begin(), res.end(), '\"'), res.end());
  return res;
}

// Add leading and trailing quotes
std::string Config::addQuotes(const std::string& value) { return "\"" + value + "\""; }

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
template <>
std::string Config::StripQuotesFromStrings<std::string>(const std::string& value) {
  return stripQuotes(value);
}

template <typename T>
T Config::StripQuotesFromStrings(const T& value) {
  return value;
}

template <typename T>
void Config::CopyFromConfig(T& dest, const std::string& option_name, boost::log::trivial::severity_level warning_level,
                            const boost::property_tree::ptree& pt) {
  boost::optional<T> value = pt.get_optional<T>(option_name);
  if (value.is_initialized()) {
    dest = StripQuotesFromStrings(value.get());
  } else {
    static boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
    BOOST_LOG_SEV(logger, static_cast<boost::log::trivial::severity_level>(warning_level))
        << option_name << " not in config file. Using default:" << dest;
  }
}

template <>
std::string Config::addQuotesToStrings<std::string>(const std::string& value) {
  return addQuotes(value);
}

template <typename T>
T Config::addQuotesToStrings(const T& value) {
  return value;
}

template <typename T>
void Config::writeOption(std::ofstream& sink, const T& data, const std::string& option_name) {
  sink << option_name << " = " << addQuotesToStrings(data) << "\n";
}

// End template tricks

Config::Config() { postUpdateValues(); }

Config::Config(const boost::filesystem::path& filename) {
  updateFromToml(filename);
  postUpdateValues();
}

Config::Config(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd) {
  updateFromToml(filename);
  updateFromCommandLine(cmd);
  postUpdateValues();
}

Config::Config(const boost::property_tree::ptree& pt) {
  updateFromPropertyTree(pt);
  postUpdateValues();
}

void Config::postUpdateValues() {
  if (provision.provision_path.empty()) {
    provision.mode = kImplicit;
  }

  if (tls.server.empty()) {
    if (!tls.server_url_path.empty()) {
      tls.server = Utils::readFile(tls.server_url_path);
    } else if (!provision.provision_path.empty()) {
      if (boost::filesystem::exists(provision.provision_path)) {
        tls.server = Bootstrap::readServerUrl(provision.provision_path);
      } else {
        LOG_ERROR << "Provided provision archive '" << provision.provision_path << "' does not exist!";
      }
    }
  }

  if (provision.server.empty()) provision.server = tls.server;

  if (uptane.repo_server.empty()) uptane.repo_server = tls.server + "/repo";

  if (uptane.director_server.empty()) uptane.director_server = tls.server + "/director";

  if (pacman.ostree_server.empty()) pacman.ostree_server = tls.server + "/treehub";
}

void Config::updateFromToml(const boost::filesystem::path& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename.string(), pt);
  updateFromPropertyTree(pt);
  LOG_TRACE << "config read from " << filename << " :\n" << (*this);
}

// For testing
void Config::updateFromTomlString(const std::string& contents) {
  boost::property_tree::ptree pt;
  std::stringstream stream(contents);
  boost::property_tree::ini_parser::read_ini(stream, pt);
  updateFromPropertyTree(pt);
}

void Config::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in config.h and writeToFile().

  CopyFromConfig(gateway.http, "gateway.http", boost::log::trivial::info, pt);
  CopyFromConfig(gateway.socket, "gateway.socket", boost::log::trivial::info, pt);

  CopyFromConfig(network.socket_commands_path, "network.socket_commands_path", boost::log::trivial::trace, pt);
  CopyFromConfig(network.socket_events_path, "network.socket_events_path", boost::log::trivial::trace, pt);

  boost::optional<std::string> events_string = pt.get_optional<std::string>("network.socket_events");
  if (events_string.is_initialized()) {
    std::string e = stripQuotes(events_string.get());
    network.socket_events.empty();
    boost::split(network.socket_events, e, boost::is_any_of(", "), boost::token_compress_on);
  } else {
    LOG_TRACE << "network.socket_events not in config file. Using default";
  }

  CopyFromConfig(p11.module, "p11.module", boost::log::trivial::trace, pt);
  CopyFromConfig(p11.pass, "p11.pass", boost::log::trivial::trace, pt);
  CopyFromConfig(p11.uptane_key_id, "p11.uptane_key_id", boost::log::trivial::warning, pt);
  CopyFromConfig(p11.tls_cacert_id, "p11.tls_cacert_id", boost::log::trivial::warning, pt);
  CopyFromConfig(p11.tls_pkey_id, "p11.tls_pkey_id", boost::log::trivial::warning, pt);
  CopyFromConfig(p11.tls_clientcert_id, "p11.tls_clientcert_id", boost::log::trivial::warning, pt);

  CopyFromConfig(tls.server, "tls.server", boost::log::trivial::warning, pt);
  CopyFromConfig(tls.server_url_path, "tls.server_url_path", boost::log::trivial::warning, pt);

  std::string tls_source = "file";
  CopyFromConfig(tls_source, "tls.ca_source", boost::log::trivial::warning, pt);
  if (tls_source == "pkcs11")
    tls.ca_source = kPkcs11;
  else
    tls.ca_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "tls.cert_source", boost::log::trivial::warning, pt);
  if (tls_source == "pkcs11")
    tls.cert_source = kPkcs11;
  else
    tls.cert_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "tls.pkey_source", boost::log::trivial::warning, pt);
  if (tls_source == "pkcs11")
    tls.pkey_source = kPkcs11;
  else
    tls.pkey_source = kFile;

  CopyFromConfig(provision.server, "provision.server", boost::log::trivial::warning, pt);
  CopyFromConfig(provision.p12_password, "provision.p12_password", boost::log::trivial::warning, pt);
  CopyFromConfig(provision.expiry_days, "provision.expiry_days", boost::log::trivial::warning, pt);
  CopyFromConfig(provision.provision_path, "provision.provision_path", boost::log::trivial::warning, pt);
  // provision.mode is set in postUpdateValues.

  CopyFromConfig(uptane.polling, "uptane.polling", boost::log::trivial::trace, pt);
  CopyFromConfig(uptane.polling_sec, "uptane.polling_sec", boost::log::trivial::trace, pt);
  CopyFromConfig(uptane.device_id, "uptane.device_id", boost::log::trivial::warning, pt);
  CopyFromConfig(uptane.primary_ecu_serial, "uptane.primary_ecu_serial", boost::log::trivial::warning, pt);
  CopyFromConfig(uptane.primary_ecu_hardware_id, "uptane.primary_ecu_hardware_id", boost::log::trivial::warning, pt);
  CopyFromConfig(uptane.director_server, "uptane.director_server", boost::log::trivial::warning, pt);
  CopyFromConfig(uptane.repo_server, "uptane.repo_server", boost::log::trivial::warning, pt);

  std::string key_source = "file";
  CopyFromConfig(key_source, "uptane.key_source", boost::log::trivial::warning, pt);
  if (key_source == "pkcs11")
    uptane.key_source = kPkcs11;
  else
    uptane.key_source = kFile;

  std::string key_type;
  CopyFromConfig(key_type, "uptane.key_type", boost::log::trivial::warning, pt);
  if (key_type.size()) {
    if (key_type == "RSA2048") {
      uptane.key_type = kRSA2048;
    } else if (key_type == "RSA4096") {
      uptane.key_type = kRSA4096;
    } else if (key_type == "ED25519") {
      uptane.key_type = kED25519;
    }
  }

  // uptane.secondary_configs currently can only be set via command line.

  std::string package_manager = "ostree";
  CopyFromConfig(package_manager, "pacman.type", boost::log::trivial::warning, pt);
  if (package_manager == "ostree") {
    pacman.type = kOstree;
  } else if (package_manager == "debian") {
    pacman.type = kDebian;
  } else {
    pacman.type = kNone;
  }

  CopyFromConfig(pacman.os, "pacman.os", boost::log::trivial::warning, pt);
  CopyFromConfig(pacman.sysroot, "pacman.sysroot", boost::log::trivial::warning, pt);
  CopyFromConfig(pacman.ostree_server, "pacman.ostree_server", boost::log::trivial::warning, pt);
  CopyFromConfig(pacman.packages_file, "pacman.packages_file", boost::log::trivial::warning, pt);

  std::string storage_type = "filesystem";
  CopyFromConfig(storage_type, "storage.type", boost::log::trivial::warning, pt);
  if (storage_type == "sqlite")
    storage.type = kSqlite;
  else
    storage.type = kFileSystem;

  CopyFromConfig(storage.uptane_metadata_path, "storage.uptane_metadata_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.uptane_private_key_path, "storage.uptane_private_key_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.uptane_public_key_path, "storage.uptane_public_key_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.tls_cacert_path, "storage.tls_cacert_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.tls_pkey_path, "storage.tls_pkey_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.tls_clientcert_path, "storage.tls_clientcert_path", boost::log::trivial::warning, pt);
  CopyFromConfig(storage.path, "storage.path", boost::log::trivial::trace, pt);
  CopyFromConfig(storage.sqldb_path, "storage.sqldb_path", boost::log::trivial::trace, pt);
  CopyFromConfig(storage.schemas_path, "storage.schemas_path", boost::log::trivial::trace, pt);

  CopyFromConfig(import.uptane_private_key_path, "import.uptane_private_key_path", boost::log::trivial::warning, pt);
  CopyFromConfig(import.uptane_public_key_path, "import.uptane_public_key_path", boost::log::trivial::warning, pt);
  CopyFromConfig(import.tls_cacert_path, "import.tls_cacert_path", boost::log::trivial::warning, pt);
  CopyFromConfig(import.tls_pkey_path, "import.tls_pkey_path", boost::log::trivial::warning, pt);
  CopyFromConfig(import.tls_clientcert_path, "import.tls_clientcert_path", boost::log::trivial::warning, pt);
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("poll-once") != 0) {
    uptane.polling = false;
  }
  if (cmd.count("gateway-http") != 0) {
    gateway.http = cmd["gateway-http"].as<bool>();
  }
  if (cmd.count("gateway-socket") != 0) {
    gateway.socket = cmd["gateway-socket"].as<bool>();
  }
  if (cmd.count("primary-ecu-serial") != 0) {
    uptane.primary_ecu_serial = cmd["primary-ecu-serial"].as<std::string>();
  }
  if (cmd.count("primary-ecu-hardware-id") != 0) {
    uptane.primary_ecu_hardware_id = cmd["primary-ecu-hardware-id"].as<std::string>();
  }
  if (cmd.count("tls-server") != 0) {
    tls.server = cmd["tls-server"].as<std::string>();
  }
  if (cmd.count("repo-server") != 0) {
    uptane.repo_server = cmd["repo-server"].as<std::string>();
  }
  if (cmd.count("director-server") != 0) {
    uptane.director_server = cmd["director-server"].as<std::string>();
  }
  if (cmd.count("ostree-server") != 0) {
    pacman.ostree_server = cmd["ostree-server"].as<std::string>();
  }

  if (cmd.count("secondary-config") != 0) {
    std::vector<boost::filesystem::path> sconfigs = cmd["secondary-config"].as<std::vector<boost::filesystem::path> >();
    readSecondaryConfigs(sconfigs);
  }

  if (cmd.count("legacy-interface") != 0) {
    boost::filesystem::path legacy_interface = cmd["legacy-interface"].as<boost::filesystem::path>();
    checkLegacyVersion(legacy_interface);
    initLegacySecondaries(legacy_interface);
  }
}

void Config::readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs) {
  std::vector<boost::filesystem::path>::const_iterator it;
  for (it = sconfigs.begin(); it != sconfigs.end(); ++it) {
    if (!boost::filesystem::exists(*it)) {
      throw FatalException(it->string() + " does not exist!");
    }
    Json::Value config_json = Utils::parseJSONFile(*it);
    Uptane::SecondaryConfig sconfig;

    std::string stype = config_json["secondary_type"].asString();
    if (stype == "virtual") {
      sconfig.secondary_type = Uptane::kVirtual;
    } else if (stype == "legacy") {
      LOG_ERROR << "Legacy secondaries should be initialized with --legacy-interface.";
      continue;
    } else if (stype == "uptane") {
      sconfig.secondary_type = Uptane::kUptane;
    } else {
      LOG_ERROR << "Unrecognized secondary type: " << stype;
      continue;
    }
    sconfig.ecu_serial = config_json["ecu_serial"].asString();
    sconfig.ecu_hardware_id = config_json["ecu_hardware_id"].asString();
    sconfig.partial_verifying = config_json["partial_verifying"].asBool();
    sconfig.ecu_private_key = config_json["ecu_private_key"].asString();
    sconfig.ecu_public_key = config_json["ecu_public_key"].asString();

    sconfig.full_client_dir = boost::filesystem::path(config_json["full_client_dir"].asString());
    sconfig.firmware_path = boost::filesystem::path(config_json["firmware_path"].asString());
    sconfig.metadata_path = boost::filesystem::path(config_json["metadata_path"].asString());
    sconfig.target_name_path = boost::filesystem::path(config_json["target_name_path"].asString());
    sconfig.flasher = "";

    std::string key_type = config_json["key_type"].asString();
    if (key_type.size()) {
      if (key_type == "RSA2048") {
        sconfig.key_type = kRSA2048;
      } else if (key_type == "RSA4096") {
        sconfig.key_type = kRSA4096;
      } else if (key_type == "ED25519") {
        sconfig.key_type = kED25519;
      }
    }
    uptane.secondary_configs.push_back(sconfig);
  }
}

void Config::checkLegacyVersion(const boost::filesystem::path& legacy_interface) {
  if (!boost::filesystem::exists(legacy_interface)) {
    throw FatalException(std::string("Legacy external flasher not found: ") + legacy_interface.string());
  }
  std::stringstream command;
  std::string output;
  command << legacy_interface << " api-version --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    throw FatalException(std::string("Legacy external flasher api-version command failed: ") + output);
  } else {
    boost::trim_if(output, boost::is_any_of(" \n\r\t"));
    if (output != "1") {
      throw FatalException(std::string("Unexpected legacy external flasher API version: ") + output);
    }
  }
}

void Config::initLegacySecondaries(const boost::filesystem::path& legacy_interface) {
  std::stringstream command;
  std::string output;
  command << legacy_interface << " list-ecus --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    LOG_ERROR << "Legacy external flasher list-ecus command failed: " << output;
    return;
  }

  std::stringstream ss(output);
  std::string buffer;
  while (std::getline(ss, buffer, '\n')) {
    Uptane::SecondaryConfig sconfig;
    sconfig.secondary_type = Uptane::kLegacy;
    std::vector<std::string> ecu_info;
    boost::split(ecu_info, buffer, boost::is_any_of(" \n\r\t"), boost::token_compress_on);
    if (ecu_info.size() == 0) {
      // Could print a warning but why bother.
      continue;
    } else if (ecu_info.size() == 1) {
      sconfig.ecu_hardware_id = ecu_info[0];
      // Use getSerial, which will get the public_key_id, initialized in ManagedSecondary constructor.
      sconfig.ecu_serial = "";
      sconfig.full_client_dir = storage.path / sconfig.ecu_hardware_id;
      LOG_INFO << "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id;
    } else if (ecu_info.size() >= 2) {
      // For now, silently ignore anything after the second token.
      sconfig.ecu_hardware_id = ecu_info[0];
      sconfig.ecu_serial = ecu_info[1];
      sconfig.full_client_dir = storage.path / (sconfig.ecu_hardware_id + "-" + sconfig.ecu_serial);
      LOG_INFO << "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id << " and serial "
               << sconfig.ecu_serial;
    }

    sconfig.partial_verifying = false;
    sconfig.ecu_private_key = "sec.private";
    sconfig.ecu_public_key = "sec.public";

    sconfig.firmware_path = sconfig.full_client_dir / "firmware.bin";
    sconfig.metadata_path = sconfig.full_client_dir / "metadata";
    sconfig.target_name_path = sconfig.full_client_dir / "target_name";
    sconfig.flasher = legacy_interface;

    uptane.secondary_configs.push_back(sconfig);
  }
}

// This writes out every configuration option, including those set with default
// and blank values. This may be useful for replicating an exact configuration
// environment. However, if we were to want to simplify the output file, we
// could skip blank strings or compare values against a freshly built instance
// to detect and skip default values.
void Config::writeToFile(const boost::filesystem::path& filename) {
  // Keep this order the same as in config.h and updateFromPropertyTree().

  std::ofstream sink(filename.c_str(), std::ofstream::out);
  sink << std::boolalpha;

  sink << "[gateway]\n";
  writeOption(sink, gateway.http, "http");
  writeOption(sink, gateway.socket, "socket");
  sink << "\n";

  sink << "[network]\n";
  writeOption(sink, network.socket_commands_path, "socket_commands_path");
  writeOption(sink, network.socket_events_path, "socket_events_path");
  std::string socket_events = "";
  for (std::vector<std::string>::iterator it = network.socket_events.begin(); it != network.socket_events.end(); ++it) {
    socket_events += *it;
    if (it != --network.socket_events.end()) {
      socket_events += ", ";
    }
  }
  writeOption(sink, socket_events, "socket_events");
  sink << "\n";

  sink << "[p11]\n";
  writeOption(sink, p11.module, "module");
  writeOption(sink, p11.pass, "pass");
  writeOption(sink, p11.uptane_key_id, "uptane_key_id");
  writeOption(sink, p11.tls_cacert_id, "tls_ca_id");
  writeOption(sink, p11.tls_pkey_id, "tls_pkey_id");
  writeOption(sink, p11.tls_clientcert_id, "tls_clientcert_id");
  sink << "\n";

  sink << "[tls]\n";
  writeOption(sink, tls.server, "server");
  writeOption(sink, tls.server_url_path, "server_url_path");
  writeOption(sink, tls.ca_source, "ca_source");
  writeOption(sink, tls.pkey_source, "pkey_source");
  writeOption(sink, tls.cert_source, "cert_source");
  sink << "\n";

  sink << "[provision]\n";
  writeOption(sink, provision.server, "server");
  writeOption(sink, provision.p12_password, "p12_password");
  writeOption(sink, provision.expiry_days, "expiry_days");
  writeOption(sink, provision.provision_path, "provision_path");
  // mode is not set directly, so don't write it out.
  sink << "\n";

  sink << "[uptane]\n";
  writeOption(sink, uptane.polling, "polling");
  writeOption(sink, uptane.polling_sec, "polling_sec");
  writeOption(sink, uptane.device_id, "device_id");
  writeOption(sink, uptane.primary_ecu_serial, "primary_ecu_serial");
  writeOption(sink, uptane.primary_ecu_hardware_id, "primary_ecu_hardware_id");
  writeOption(sink, uptane.director_server, "director_server");
  writeOption(sink, uptane.repo_server, "repo_server");
  writeOption(sink, uptane.key_source, "key_source");
  writeOption(sink, uptane.key_type, "key_type");
  // uptane.secondary_configs currently can only be set via command line and is
  // not read from or written to the primary config file.
  sink << "\n";

  sink << "[pacman]\n";
  writeOption(sink, pacman.type, "type");
  writeOption(sink, pacman.os, "os");
  writeOption(sink, pacman.sysroot, "sysroot");
  writeOption(sink, pacman.ostree_server, "ostree_server");
  writeOption(sink, pacman.packages_file, "packages_file");
  sink << "\n";

  sink << "[storage]\n";
  writeOption(sink, storage.type, "type");
  writeOption(sink, storage.path, "path");
  writeOption(sink, storage.uptane_metadata_path, "uptane_metadata_path");
  writeOption(sink, storage.uptane_private_key_path, "uptane_private_key_path");
  writeOption(sink, storage.uptane_public_key_path, "uptane_public_key_path");
  writeOption(sink, storage.tls_cacert_path, "tls_cacert_path");
  writeOption(sink, storage.tls_pkey_path, "tls_pkey_path");
  writeOption(sink, storage.tls_clientcert_path, "tls_clientcert_path");
  sink << "\n";

  sink << "[import]\n";
  writeOption(sink, import.uptane_private_key_path, "uptane_private_key_path");
  writeOption(sink, import.uptane_public_key_path, "uptane_public_key_path");
  writeOption(sink, import.tls_cacert_path, "tls_cacert_path");
  writeOption(sink, import.tls_pkey_path, "tls_pkey_path");
  writeOption(sink, import.tls_clientcert_path, "tls_clientcert_path");
  sink << "\n";
}

std::string TlsConfig::cer_serialize() {
  std::string res;
  res = cer_encode_sequence() + cer_encode_string(server, kAsn1Utf8String) +
        cer_encode_string(server_url_path.string(), kAsn1Utf8String) + cer_encode_integer(ca_source, kAsn1Enum) +
        cer_encode_integer(pkey_source, kAsn1Enum) + cer_encode_integer(cert_source, kAsn1Enum) + cer_encode_endcons();
  return res;
}

void TlsConfig::cer_deserialize(const std::string& cer) {
  std::string cer_local = cer;

  int32_t sequence_length;
  cer_local = cer_decode_except_crop(cer_local, &sequence_length, nullptr, kSequence);

  if (sequence_length != -1) cer_local = cer_local.substr(0, sequence_length);

  cer_local = cer_decode_except_crop(cer_local, nullptr, &server, kOctetString);

  {
    std::string url_path;
    cer_local = cer_decode_except_crop(cer_local, nullptr, &url_path, kOctetString);
    server_url_path = url_path;
  }

  cer_local = cer_decode_except_crop_enum(cer_local, &ca_source, kFile, kPkcs11);

  cer_local = cer_decode_except_crop_enum(cer_local, &pkey_source, kFile, kPkcs11);

  cer_local = cer_decode_except_crop_enum(cer_local, &cert_source, kFile, kPkcs11);

  // Extra elements of the sequence are ignored
  if (sequence_length == -1 && !cer_local.empty()) {
    int nestedness = 0;
    for (;;) {
      int32_t endpos;
      int32_t int_param;
      int type = cer_decode_token(cer_local, &endpos, &int_param, nullptr);
      cer_local = cer_local.substr(endpos);

      switch (type) {
        case kUnknown:
          throw deserialization_error();

        case kEndSequence:
          if (nestedness == 0)
            return;
          else
            --nestedness;
          break;

        case kSequence:
          if (int_param == -1)  // indefinite length, matching 0000 should follow
            ++nestedness;
          break;
      }
    }
  }
}
