#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

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

std::string TlsConfig::ca_file() const {
  boost::filesystem::path ca_path(ca_file_);
  if (ca_path.is_absolute() || ca_source == kPkcs11) {
    return ca_file_;
  } else {
    return (certificates_directory / ca_path).string();
  }
}

std::string TlsConfig::pkey_file() const {
  boost::filesystem::path pkey_path(pkey_file_);
  if (pkey_path.is_absolute() || cert_source == kPkcs11) {
    return pkey_file_;
  } else {
    return (certificates_directory / pkey_path).string();
  }
}

std::string TlsConfig::client_certificate() const {
  boost::filesystem::path cert_path(client_certificate_);
  if (cert_path.is_absolute() || pkey_source == kPkcs11) {
    return client_certificate_;
  } else {
    return (certificates_directory / cert_path).string();
  }
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

  CopyFromConfig(tls.certificates_directory, "tls.certificates_directory", LVL_trace, pt);
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

  CopyFromConfig(tls.ca_file_, "tls.ca_file", LVL_warning, pt);
  CopyFromConfig(tls.pkey_file_, "tls.pkey_file", LVL_warning, pt);
  CopyFromConfig(tls.client_certificate_, "tls.client_certificate", LVL_warning, pt);

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
  CopyFromConfig(uptane.metadata_path, "uptane.metadata_path", LVL_warning, pt);

  std::string key_source = "file";
  CopyFromConfig(key_source, "uptane.key_source", LVL_warning, pt);
  if (key_source == "pkcs11")
    uptane.key_source = kPkcs11;
  else
    uptane.key_source = kFile;

  CopyFromConfig(uptane.private_key_path, "uptane.private_key_path", LVL_warning, pt);
  CopyFromConfig(uptane.public_key_path, "uptane.public_key_path", LVL_warning, pt);

  CopyFromConfig(ostree.os, "ostree.os", LVL_warning, pt);
  CopyFromConfig(ostree.sysroot, "ostree.sysroot", LVL_warning, pt);
  CopyFromConfig(ostree.packages_file, "ostree.packages_file", LVL_warning, pt);
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

  // temporary
  if (cmd.count("secondary-config") != 0) {
    std::vector<std::string> configs = cmd["secondary-config"].as<std::vector<std::string> >();
    std::vector<std::string>::iterator it;
    for (it = configs.begin(); it != configs.end(); ++it) {
      Json::Value config_json = Utils::parseJSONFile(*it);
      Uptane::SecondaryConfig ecu_config;
      ecu_config.full_client_dir = boost::filesystem::path(config_json["full_client_dir"].asString());
      ecu_config.ecu_serial = config_json["ecu_serial"].asString();
      ecu_config.ecu_hardware_id = config_json["ecu_hardware_id"].asString();
      ecu_config.ecu_private_key = config_json["ecu_private_key"].asString();
      ecu_config.ecu_public_key = config_json["ecu_public_key"].asString();
      ecu_config.firmware_path = config_json["firmware_path"].asString();
      uptane.secondaries.push_back(ecu_config);
    }
  }
}

// This writes out every configuration option, including those set with default
// and blank values. This may be useful to replicating an exact configuration
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
  sink << "\n";

  sink << "[tls]\n";
  writeOption(sink, tls.certificates_directory, "certificates_directory");
  writeOption(sink, tls.server, "server");
  writeOption(sink, tls.ca_file_, "ca_file");
  writeOption(sink, tls.ca_source, "ca_source");
  writeOption(sink, tls.pkey_file_, "pkey_file");
  writeOption(sink, tls.pkey_source, "pkey_source");
  writeOption(sink, tls.client_certificate_, "client_certificate");
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
  writeOption(sink, uptane.metadata_path, "metadata_path");
  writeOption(sink, uptane.private_key_path, "private_key_path");
  writeOption(sink, uptane.public_key_path, "public_key_path");
  writeOption(sink, uptane.key_source, "key_source");
  // TODO: Handle vector<UptaneSecondaryConfig>:
  // writeOption(sink, uptane.secondaries, "secondaries");
  sink << "\n";

  sink << "[ostree]\n";
  writeOption(sink, ostree.os, "os");
  writeOption(sink, ostree.sysroot, "sysroot");
  writeOption(sink, ostree.packages_file, "packages_file");
  sink << "\n";
}
