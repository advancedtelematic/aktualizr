#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>

#include "bootstrap.h"
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

std::ostream& operator<<(std::ostream& os, StorageType cs) {
  std::string cs_str;
  switch (cs) {
    case kFileSystem:
      cs_str = "filesystem";
      break;
    case kSqlite:
      cs_str = "sqlite";
      break;
    default:
      cs_str = "unknown";
      break;
  }
  os << '"' << cs_str << '"';
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
void Config::CopyFromConfig(T& dest, const std::string& option_name, LoggerLevels warning_level,
                            const boost::property_tree::ptree& pt) {
  boost::optional<T> value = pt.get_optional<T>(option_name);
  if (value.is_initialized()) {
    dest = StripQuotesFromStrings(value.get());
  } else {
    LOGGER_LOG(warning_level, option_name << " not in config file. Using default:" << dest);
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

Config::Config(const std::string& filename) {
  updateFromToml(filename);
  postUpdateValues();
}

Config::Config(const std::string& filename, const boost::program_options::variables_map& cmd) {
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
    if (!provision.provision_path.empty()) {
      if (boost::filesystem::exists(provision.provision_path)) {
        tls.server = Bootstrap::readServerUrl(provision.provision_path);
      } else {
        LOGGER_LOG(LVL_error, "Provided provision archive '" << provision.provision_path << "' does not exist!");
      }
    }
  }

  if (provision.server.empty()) provision.server = tls.server;

  if (uptane.repo_server.empty()) uptane.repo_server = tls.server + "/repo";

  if (uptane.director_server.empty()) uptane.director_server = tls.server + "/director";

  if (uptane.ostree_server.empty()) uptane.ostree_server = tls.server + "/treehub";
}

void Config::updateFromToml(const std::string& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename, pt);
  updateFromPropertyTree(pt);
  LOGGER_LOG(LVL_trace, "config read from " << filename << " :\n" << (*this));
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

#ifdef WITH_GENIVI
  CopyFromConfig(dbus.software_manager, "dbus.software_manager", LVL_trace, pt);
  CopyFromConfig(dbus.software_manager_path, "dbus.software_manager_path", LVL_trace, pt);
  CopyFromConfig(dbus.path, "dbus.path", LVL_trace, pt);
  CopyFromConfig(dbus.interface, "dbus.interface", LVL_trace, pt);
  CopyFromConfig(dbus.timeout, "dbus.timeout", LVL_trace, pt);
  boost::optional<std::string> dbus_type = pt.get_optional<std::string>("dbus.bus");
  if (dbus_type.is_initialized()) {
    std::string bus = stripQuotes(dbus_type.get());
    if (bus == "system") {
      dbus.bus = DBUS_BUS_SYSTEM;
    } else if (bus == "session") {
      dbus.bus = DBUS_BUS_SESSION;
    } else {
      LOGGER_LOG(LVL_error, "Unrecognised value for dbus.bus:" << bus);
    }
  } else {
    LOGGER_LOG(LVL_trace, "dbus.bus not in config file. Using default");
  }
#endif

  CopyFromConfig(gateway.http, "gateway.http", LVL_info, pt);
  CopyFromConfig(gateway.rvi, "gateway.rvi", LVL_info, pt);
  CopyFromConfig(gateway.socket, "gateway.socket", LVL_info, pt);
  CopyFromConfig(gateway.dbus, "gateway.dbus", LVL_info, pt);

  CopyFromConfig(network.socket_commands_path, "network.socket_commands_path", LVL_trace, pt);
  CopyFromConfig(network.socket_events_path, "network.socket_events_path", LVL_trace, pt);

  boost::optional<std::string> events_string = pt.get_optional<std::string>("network.socket_events");
  if (events_string.is_initialized()) {
    std::string e = stripQuotes(events_string.get());
    network.socket_events.empty();
    boost::split(network.socket_events, e, boost::is_any_of(", "), boost::token_compress_on);
  } else {
    LOGGER_LOG(LVL_trace, "network.socket_events not in config file. Using default");
  }

  CopyFromConfig(rvi.node_host, "rvi.node_host", LVL_warning, pt);
  CopyFromConfig(rvi.node_port, "rvi.node_port", LVL_trace, pt);
  CopyFromConfig(rvi.device_key, "rvi.device_key", LVL_warning, pt);
  CopyFromConfig(rvi.device_cert, "rvi.device_cert", LVL_warning, pt);
  CopyFromConfig(rvi.ca_cert, "rvi.ca_cert", LVL_warning, pt);
  CopyFromConfig(rvi.cert_dir, "rvi.cert_dir", LVL_warning, pt);
  CopyFromConfig(rvi.cred_dir, "rvi.cred_dir", LVL_warning, pt);
  CopyFromConfig(rvi.packages_dir, "rvi.packages_dir", LVL_trace, pt);
  CopyFromConfig(rvi.uuid, "rvi.uuid", LVL_warning, pt);

  CopyFromConfig(p11.module, "p11.module", LVL_trace, pt);
  CopyFromConfig(p11.pass, "p11.pass", LVL_trace, pt);
  CopyFromConfig(p11.uptane_key_id, "p11.uptane_key_id", LVL_warning, pt);
  CopyFromConfig(p11.tls_cacert_id, "p11.tls_cacert_id", LVL_warning, pt);
  CopyFromConfig(p11.tls_pkey_id, "p11.tls_pkey_id", LVL_warning, pt);
  CopyFromConfig(p11.tls_clientcert_id, "p11.tls_clientcert_id", LVL_warning, pt);

  CopyFromConfig(tls.server, "tls.server", LVL_warning, pt);

  std::string tls_source = "file";
  CopyFromConfig(tls_source, "tls.ca_source", LVL_warning, pt);
  if (tls_source == "pkcs11")
    tls.ca_source = kPkcs11;
  else
    tls.ca_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "tls.cert_source", LVL_warning, pt);
  if (tls_source == "pkcs11")
    tls.cert_source = kPkcs11;
  else
    tls.cert_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "tls.pkey_source", LVL_warning, pt);
  if (tls_source == "pkcs11")
    tls.pkey_source = kPkcs11;
  else
    tls.pkey_source = kFile;

  CopyFromConfig(provision.server, "provision.server", LVL_warning, pt);
  CopyFromConfig(provision.p12_password, "provision.p12_password", LVL_warning, pt);
  CopyFromConfig(provision.expiry_days, "provision.expiry_days", LVL_warning, pt);
  CopyFromConfig(provision.provision_path, "provision.provision_path", LVL_warning, pt);
  // provision.mode is set in postUpdateValues.

  CopyFromConfig(uptane.polling, "uptane.polling", LVL_trace, pt);
  CopyFromConfig(uptane.polling_sec, "uptane.polling_sec", LVL_trace, pt);
  CopyFromConfig(uptane.device_id, "uptane.device_id", LVL_warning, pt);
  CopyFromConfig(uptane.primary_ecu_serial, "uptane.primary_ecu_serial", LVL_warning, pt);
  CopyFromConfig(uptane.primary_ecu_hardware_id, "uptane.primary_ecu_hardware_id", LVL_warning, pt);
  CopyFromConfig(uptane.ostree_server, "uptane.ostree_server", LVL_warning, pt);
  CopyFromConfig(uptane.director_server, "uptane.director_server", LVL_warning, pt);
  CopyFromConfig(uptane.repo_server, "uptane.repo_server", LVL_warning, pt);

  std::string key_source = "file";
  CopyFromConfig(key_source, "uptane.key_source", LVL_warning, pt);
  if (key_source == "pkcs11")
    uptane.key_source = kPkcs11;
  else
    uptane.key_source = kFile;

  // uptane.secondary_configs currently can only be set via command line.

  CopyFromConfig(ostree.os, "ostree.os", LVL_warning, pt);
  CopyFromConfig(ostree.sysroot, "ostree.sysroot", LVL_warning, pt);
  CopyFromConfig(ostree.packages_file, "ostree.packages_file", LVL_warning, pt);

  std::string storage_type = "filesystem";
  CopyFromConfig(storage_type, "storage.type", LVL_warning, pt);
  if (storage_type == "sqlite")
    storage.type = kSqlite;
  else
    storage.type = kFileSystem;

  CopyFromConfig(storage.uptane_metadata_path, "storage.uptane_metadata_path", LVL_warning, pt);
  CopyFromConfig(storage.uptane_private_key_path, "storage.uptane_private_key_path", LVL_warning, pt);
  CopyFromConfig(storage.uptane_public_key_path, "storage.uptane_public_key_path", LVL_warning, pt);
  CopyFromConfig(storage.tls_cacert_path, "storage.tls_cacert_path", LVL_warning, pt);
  CopyFromConfig(storage.tls_pkey_path, "storage.tls_pkey_path", LVL_warning, pt);
  CopyFromConfig(storage.tls_clientcert_path, "storage.tls_clientcert_path", LVL_warning, pt);
  CopyFromConfig(storage.path, "storage.path", LVL_trace, pt);
  CopyFromConfig(storage.sqldb_path, "storage.sqldb_path", LVL_trace, pt);
  CopyFromConfig(storage.schema_version, "storage.schema_version", LVL_trace, pt);
  CopyFromConfig(storage.schemas_path, "storage.schemas_path", LVL_trace, pt);

  CopyFromConfig(import.uptane_private_key_path, "import.uptane_private_key_path", LVL_warning, pt);
  CopyFromConfig(import.uptane_public_key_path, "import.uptane_public_key_path", LVL_warning, pt);
  CopyFromConfig(import.tls_cacert_path, "import.tls_cacert_path", LVL_warning, pt);
  CopyFromConfig(import.tls_pkey_path, "import.tls_pkey_path", LVL_warning, pt);
  CopyFromConfig(import.tls_clientcert_path, "import.tls_clientcert_path", LVL_warning, pt);
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("poll-once") != 0) {
    uptane.polling = false;
  }
  if (cmd.count("gateway-http") != 0) {
    gateway.http = cmd["gateway-http"].as<bool>();
  }
  if (cmd.count("gateway-rvi") != 0) {
    gateway.rvi = cmd["gateway-rvi"].as<bool>();
  }
  if (cmd.count("gateway-dbus") != 0) {
    gateway.dbus = cmd["gateway-dbus"].as<bool>();
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
    uptane.ostree_server = cmd["ostree-server"].as<std::string>();
  }

  if (cmd.count("secondary-config") != 0) {
    std::vector<std::string> sconfigs = cmd["secondary-config"].as<std::vector<std::string> >();
    readSecondaryConfigs(sconfigs);
  }

  if (cmd.count("legacy-interface") != 0) {
    std::string legacy_interface = cmd["legacy-interface"].as<std::string>();
    if (checkLegacyVersion(legacy_interface)) {
      initLegacySecondaries(legacy_interface);
    }
  }
}

void Config::readSecondaryConfigs(const std::vector<std::string>& sconfigs) {
  std::vector<std::string>::const_iterator it;
  for (it = sconfigs.begin(); it != sconfigs.end(); ++it) {
    Json::Value config_json = Utils::parseJSONFile(*it);
    Uptane::SecondaryConfig sconfig;

    std::string stype = config_json["secondary_type"].asString();
    if (stype == "virtual") {
      sconfig.secondary_type = Uptane::kVirtual;
    } else if (stype == "legacy") {
      LOGGER_LOG(LVL_error, "Legacy secondaries should be initialized with --legacy-interface.");
      continue;
    } else if (stype == "uptane") {
      sconfig.secondary_type = Uptane::kUptane;
    } else {
      LOGGER_LOG(LVL_error, "Unrecognized secondary type: " << stype);
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

    uptane.secondary_configs.push_back(sconfig);
  }
}

bool Config::checkLegacyVersion(const std::string& legacy_interface) {
  bool ret = false;
  if (!boost::filesystem::exists(legacy_interface)) {
    LOGGER_LOG(LVL_error, "Legacy external flasher not found: " << legacy_interface);
    return ret;
  }
  std::stringstream command;
  std::string output;
  command << legacy_interface << " api-version --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    LOGGER_LOG(LVL_error, "Legacy external flasher api-version command failed: " << output);
  } else {
    boost::trim_if(output, boost::is_any_of(" \n\r\t"));
    if (output != "1") {
      LOGGER_LOG(LVL_error, "Unexpected legacy external flasher API version: " << output);
    } else {
      ret = true;
    }
  }
  return ret;
}

void Config::initLegacySecondaries(const std::string& legacy_interface) {
  std::stringstream command;
  std::string output;
  output = std::string();
  command << legacy_interface << " list-ecus --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    LOGGER_LOG(LVL_error, "Legacy external flasher list-ecus command failed: " << output);
    return;
  }

  std::stringstream ss(output);
  std::string buffer;
  unsigned int index = 0;
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
      sconfig.ecu_serial = "";  // Initialized in ManagedSecondary constructor.
      LOGGER_LOG(LVL_info, "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id);
    } else if (ecu_info.size() >= 2) {
      // For now, silently ignore anything after the second token.
      sconfig.ecu_hardware_id = ecu_info[0];
      sconfig.ecu_serial = ecu_info[1];
      LOGGER_LOG(LVL_info, "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id << " and serial "
                                                                     << sconfig.ecu_serial);
    }

    sconfig.partial_verifying = false;
    sconfig.ecu_private_key = "sec.private";
    sconfig.ecu_public_key = "sec.public";

    sconfig.full_client_dir = storage.path / (sconfig.ecu_hardware_id + "-" + Utils::intToString(++index));
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
void Config::writeToFile(const std::string& filename) {
  // Keep this order the same as in config.h and updateFromPropertyTree().

  std::ofstream sink(filename.c_str(), std::ofstream::out);
  sink << std::boolalpha;

#ifdef WITH_GENIVI
  sink << "[dbus]\n";
  writeOption(sink, dbus.software_manager, "software_manager");
  writeOption(sink, dbus.software_manager_path, "software_manager_path");
  writeOption(sink, dbus.path, "path");
  writeOption(sink, dbus.interface, "interface");
  writeOption(sink, dbus.timeout, "timeout");
  if (dbus.bus == DBUS_BUS_SYSTEM) {
    writeOption(sink, std::string("system"), "bus");
  } else if (dbus.bus == DBUS_BUS_SESSION) {
    writeOption(sink, std::string("session"), "bus");
  }
  sink << "\n";
#endif

  sink << "[gateway]\n";
  writeOption(sink, gateway.http, "http");
  writeOption(sink, gateway.rvi, "rvi");
  writeOption(sink, gateway.socket, "socket");
  writeOption(sink, gateway.dbus, "dbus");
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

  sink << "[rvi]\n";
  writeOption(sink, rvi.node_host, "node_host");
  writeOption(sink, rvi.node_port, "node_port");
  writeOption(sink, rvi.device_key, "device_key");
  writeOption(sink, rvi.device_cert, "device_cert");
  writeOption(sink, rvi.ca_cert, "ca_cert");
  writeOption(sink, rvi.cert_dir, "cert_dir");
  writeOption(sink, rvi.cred_dir, "cred_dir");
  writeOption(sink, rvi.packages_dir, "packages_dir");
  writeOption(sink, rvi.uuid, "uuid");
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
  writeOption(sink, uptane.ostree_server, "ostree_server");
  writeOption(sink, uptane.director_server, "director_server");
  writeOption(sink, uptane.repo_server, "repo_server");
  writeOption(sink, uptane.key_source, "key_source");
  // uptane.secondary_configs currently can only be set via command line and is
  // not read from or written to the primary config file.
  sink << "\n";

  sink << "[ostree]\n";
  writeOption(sink, ostree.os, "os");
  writeOption(sink, ostree.sysroot, "sysroot");
  writeOption(sink, ostree.packages_file, "packages_file");
  sink << "\n";

  sink << "[storage]\n";
  writeOption(sink, storage.type, "type");
  writeOption(sink, storage.path, "path");
  writeOption(sink, storage.schema_version, "schema_version");
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
