#include "config.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <sstream>

#include "logger.h"
#include "utils.h"

/**
 * \par Description:
 *    Overload the << operator for the configuration class allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const Config& cfg) {
  return os << "\tAuthentication Server: " << cfg.auth.server << std::endl
            << "\tSota Server: " << cfg.core.server << std::endl
            << "\tPooling enabled : " << cfg.core.polling << std::endl
            << "\tPooling interval : " << cfg.core.polling_sec << std::endl
            << "\tClient: " << cfg.auth.client_id << std::endl
            << "\tDevice UUID: " << cfg.device.uuid;
}

// Strip leading and trailing quotes
std::string strip_quotes(const std::string value) {
  std::string res = value;
  res.erase(std::remove(res.begin(), res.end(), '\"'), res.end());
  return res;
}

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
T StripQuotesFromStrings(const T& value);

template <>
std::string StripQuotesFromStrings<std::string>(const std::string& value) {
  return strip_quotes(value);
}

template <typename T>
T StripQuotesFromStrings(const T& value) {
  return value;
}

template <typename T>
void CopyFromConfig(T& dest, const std::string& option_name, LoggerLevels warning_level,
                    const boost::property_tree::ptree& pt) {
  boost::optional<T> value = pt.get_optional<T>(option_name);
  if (value.is_initialized()) {
    dest = StripQuotesFromStrings(value.get());
  } else {
    LOGGER_LOG(warning_level, option_name << " not in config file. Using default:" << dest);
  }
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

void Config::postUpdateValues() {
  if (!tls.server.empty() && !tls.ca_file.empty() && !tls.client_certificate.empty()) {
    core.auth_type = CERTIFICATE;
    if (!auth.client_id.empty() || !auth.client_secret.empty()) {
      throw std::logic_error(
          "It is not possible to set [tls] section with 'auth.client_id' or 'auth.client_secret' properties");
    }
  } else {
    core.auth_type = OAUTH2;
  }

  boost::system::error_code error;
  boost::filesystem::create_directories(device.certificates_directory, error);
  if (error.value()) {
    throw std::runtime_error(
        std::string("Could not create directory for 'device.certificates_directory' option because: ") +
        error.message());
  }
  if (uptane.repo_server.empty()) {
    uptane.repo_server = tls.server + "/repo";
  }
  if (uptane.director_server.empty()) {
    uptane.director_server = tls.server + "/director";
  }
  if (uptane.ostree_server.empty()) {
    uptane.ostree_server = tls.server + "/treehub";
  }
  std::string ecu_serial_filename = "primary_ecu_serial";

  if (!uptane.primary_ecu_serial.empty()) {
    Utils::writeFile((device.certificates_directory / ecu_serial_filename).string(), uptane.primary_ecu_serial);
  } else {
    if (boost::filesystem::exists(device.certificates_directory / ecu_serial_filename)) {
      uptane.primary_ecu_serial = Utils::readFile((device.certificates_directory / ecu_serial_filename).string());
    } else {
      uptane.primary_ecu_serial = Utils::randomUuid();
      Utils::writeFile((device.certificates_directory / ecu_serial_filename).string(), uptane.primary_ecu_serial);
    }
  }

  std::string device_id_filename = "device_id";
  if (!uptane.device_id.empty()) {
    Utils::writeFile((device.certificates_directory / device_id_filename).string(), uptane.device_id);
  } else {
    if (boost::filesystem::exists(device.certificates_directory / device_id_filename)) {
      uptane.device_id = Utils::readFile((device.certificates_directory / device_id_filename).string());
    } else {
      uptane.device_id = Utils::genPrettyName();
      Utils::writeFile((device.certificates_directory / device_id_filename).string(), uptane.device_id);
    }
  }
  uptane.public_key_path = (device.certificates_directory / uptane.public_key_path).string();
  uptane.private_key_path = (device.certificates_directory / uptane.private_key_path).string();
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
  // Keep this order the same as in config.h
  CopyFromConfig(core.server, "core.server", LVL_debug, pt);
  CopyFromConfig(core.polling, "core.polling", LVL_trace, pt);
  CopyFromConfig(core.polling_sec, "core.polling_sec", LVL_trace, pt);

  CopyFromConfig(auth.server, "auth.server", LVL_info, pt);
  CopyFromConfig(auth.client_id, "auth.client_id", LVL_warning, pt);
  CopyFromConfig(auth.client_secret, "auth.client_secret", LVL_warning, pt);

#ifdef WITH_GENIVI
  CopyFromConfig(dbus.software_manager, "dbus.software_manager", LVL_trace, pt);
  CopyFromConfig(dbus.software_manager_path, "dbus.software_manager_path", LVL_trace, pt);
  CopyFromConfig(dbus.path, "dbus.path", LVL_trace, pt);
  CopyFromConfig(dbus.interface, "dbus.interface", LVL_trace, pt);
  CopyFromConfig(dbus.timeout, "dbus.timeout", LVL_trace, pt);
  boost::optional<std::string> dbus_type = pt.get_optional<std::string>("dbus.bus");
  if (dbus_type.is_initialized()) {
    std::string bus = strip_quotes(dbus_type.get());
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

  CopyFromConfig(device.uuid, "device.uuid", LVL_warning, pt);
  CopyFromConfig(device.packages_dir, "device.packages_dir", LVL_trace, pt);
  CopyFromConfig(device.certificates_directory, "device.certificates_directory", LVL_trace, pt);
  if (pt.get_optional<std::string>("device.package_manager").is_initialized()) {
    std::string pm = strip_quotes(pt.get_optional<std::string>("device.package_manager").get());
    if (pm == "ostree") {
      device.package_manager = PMOSTREE;
    }
  }

  CopyFromConfig(gateway.http, "gateway.http", LVL_info, pt);
  CopyFromConfig(gateway.rvi, "gateway.rvi", LVL_info, pt);
  CopyFromConfig(gateway.socket, "gateway.socket", LVL_info, pt);
  CopyFromConfig(gateway.dbus, "gateway.dbus", LVL_info, pt);

  CopyFromConfig(network.socket_commands_path, "network.socket_commands_path", LVL_trace, pt);
  CopyFromConfig(network.socket_events_path, "network.socket_events_path", LVL_trace, pt);

  boost::optional<std::string> events_string = pt.get_optional<std::string>("network.socket_events");
  if (events_string.is_initialized()) {
    std::string e = strip_quotes(events_string.get());
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

  CopyFromConfig(tls.server, "tls.server", LVL_warning, pt);
  CopyFromConfig(tls.ca_file, "tls.ca_file", LVL_warning, pt);
  CopyFromConfig(tls.client_certificate, "tls.client_certificate", LVL_warning, pt);
  CopyFromConfig(tls.pkey_file, "tls.pkey_file", LVL_warning, pt);

  CopyFromConfig(provision.p12_path, "provision.p12_path", LVL_warning, pt);
  CopyFromConfig(provision.p12_password, "provision.p12_password", LVL_warning, pt);

  CopyFromConfig(uptane.director_server, "uptane.director_server", LVL_warning, pt);
  CopyFromConfig(uptane.disable_keyid_validation, "uptane.disable_keyid_validation", LVL_debug, pt);
  CopyFromConfig(uptane.device_id, "uptane.device_id", LVL_warning, pt);
  CopyFromConfig(uptane.primary_ecu_serial, "uptane.primary_ecu_serial", LVL_warning, pt);
  CopyFromConfig(uptane.primary_ecu_hardware_id, "uptane.primary_ecu_hardware_id", LVL_warning, pt);

  CopyFromConfig(uptane.ostree_server, "uptane.ostree_server", LVL_warning, pt);

  CopyFromConfig(uptane.repo_server, "uptane.repo_server", LVL_warning, pt);
  CopyFromConfig(uptane.metadata_path, "uptane.metadata_path", LVL_warning, pt);
  CopyFromConfig(uptane.private_key_path, "uptane.private_key_path", LVL_warning, pt);
  CopyFromConfig(uptane.public_key_path, "uptane.public_key_path", LVL_warning, pt);

  CopyFromConfig(ostree.os, "ostree.os", LVL_warning, pt);
  CopyFromConfig(ostree.sysroot, "ostree.sysroot", LVL_warning, pt);
  CopyFromConfig(ostree.packages_file, "ostree.packages_file", LVL_warning, pt);
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("poll-once") != 0) {
    core.polling = false;
  }
  if (cmd.count("gateway-http") != 0) {
    gateway.http = cmd["gateway-http"].as<bool>();
  }
  if (cmd.count("gateway-rvi") != 0) {
    gateway.rvi = cmd["gateway-rvi"].as<bool>();
  }
  if (cmd.count("gateway-socket") != 0) {
    gateway.socket = cmd["gateway-socket"].as<bool>();
  }
  if (cmd.count("gateway-dbus") != 0) {
    gateway.dbus = cmd["gateway-dbus"].as<bool>();
  }
  if (cmd.count("disable-keyid-validation") != 0) {
    uptane.disable_keyid_validation = true;
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
      uptane.secondaries.push_back(ecu_config);
    }
  }
}
