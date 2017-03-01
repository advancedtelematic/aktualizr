#include "config.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "logger.h"

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
                    boost::property_tree::ptree& pt) {
  boost::optional<T> value = pt.get_optional<T>(option_name);
  if (value.is_initialized()) {
    dest = StripQuotesFromStrings(value.get());
  } else {
    LOGGER_LOG(warning_level, option_name << " not in config file. Using default:" << dest);
  }
}

// End template tricks

void Config::updateFromToml(const std::string& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename, pt);

  // Keep this order the same as in config.h
  CopyFromConfig(core.server, "core.server", LVL_debug, pt);
  CopyFromConfig(core.polling, "core.polling", LVL_trace, pt);
  CopyFromConfig(core.polling_sec, "core.polling_sec", LVL_trace, pt);

  CopyFromConfig(auth.server, "auth.server", LVL_info, pt);
  CopyFromConfig(auth.client_id, "auth.client_id", LVL_warning, pt);
  CopyFromConfig(auth.client_secret, "auth.client_secret", LVL_warning, pt);

  CopyFromConfig(dbus.software_manager, "dbus.software_manager", LVL_trace, pt);
  CopyFromConfig(dbus.software_manager_path, "dbus.software_manager_path", LVL_trace, pt);
  CopyFromConfig(dbus.path, "dbus.path", LVL_trace, pt);
  CopyFromConfig(dbus.interface, "dbus.interface", LVL_trace, pt);
  CopyFromConfig(dbus.timeout, "dbus.timeout", LVL_trace, pt);

  CopyFromConfig(device.uuid, "device.uuid", LVL_warning, pt);
  CopyFromConfig(device.packages_dir, "device.packages_dir", LVL_trace, pt);

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
  CopyFromConfig(rvi.client_config, "rvi.client_config", LVL_warning, pt);

  LOGGER_LOG(LVL_trace, "config read from " << filename << " :\n" << (*this));
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
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
}
