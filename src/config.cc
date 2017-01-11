#include "config.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "logger.h"

std::string getStringValue(boost::property_tree::ptree pt,
                           const std::string& name) {
  std::string ret_val = pt.get<std::string>(name);
  ret_val.erase(std::remove(ret_val.begin(), ret_val.end(), '\"'),
                ret_val.end());
  return ret_val;
}

/**
 * \par Description:
 *    Overload the << operator for the configuration class allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const Config& cfg) {
  return os << "\tAuthtentification Server: " << cfg.auth.server << std::endl
            << "\tSota Server: " << cfg.core.server << std::endl
            << "\tPooling enabled : " << cfg.core.polling << std::endl
            << "\tPooling interval : " << cfg.core.polling_sec << std::endl
            << "\tClient: " << cfg.auth.client_id << std::endl
            << "\tSecret: " << cfg.auth.client_secret << std::endl
            << "\tDevice UUID: " << cfg.device.uuid;
}

void Config::updateFromToml(const std::string& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename, pt);

  // CoreConfig
  try {
    core.server = getStringValue(pt, "core.server");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'core.server' option have been found in config file: "
                   << filename << ", Falling back to default: " << core.server);
  }

  try {
    core.polling = pt.get<bool>("core.polling");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'core.polling' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << core.polling);
  }

  try {
    core.polling_sec = pt.get<unsigned long long>("core.polling_sec");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'core.polling_sec' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << core.polling_sec);
  }

  // AuthConfig
  try {
    auth.server = getStringValue(pt, "auth.server");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.server' option have been found in config file: "
                   << filename << ", Falling back to default: " << auth.server);
  }

  try {
    auth.client_id = getStringValue(pt, "auth.client_id");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.client_id' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << auth.client_id);
  }

  try {
    auth.client_secret = getStringValue(pt, "auth.client_secret");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.client_secret' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << auth.client_secret);
  }

  // DeviceConfig
  try {
    device.uuid = getStringValue(pt, "device.uuid");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'device.uuid' option have been found in config file: "
                   << filename << ", Falling back to default: " << device.uuid);
  }

  try {
    device.packages_dir = getStringValue(pt, "device.packages_dir");
  } catch (...) {
    LOGGER_LOG(
        LVL_debug,
        "no 'device.packages_dir' option have been found in config file: "
            << filename
            << ", Falling back to default: " << device.packages_dir);
  }

  try {
    rvi.node_host = getStringValue(pt, "rvi.node_host");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'rvi.node_host' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << rvi.node_host);
  }

  try {
    rvi.node_port = getStringValue(pt, "rvi.node_port");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'rvi.node_port' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << rvi.node_port);
  }

  try {
    rvi.client_config = getStringValue(pt, "rvi.client_config");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'rvi.client_config' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << rvi.client_config);
  }

  try {
    std::string events_string = getStringValue(pt, "network.socket_events");
    network.socket_events.empty();
    boost::split(network.socket_events, events_string, boost::is_any_of(", "),
                 boost::token_compress_on);
  } catch (...) {
    LOGGER_LOG(
        LVL_debug,
        "no 'network.socket_events' option have been found in config file: "
            << filename << ", Falling back to default value");
  }

  LOGGER_LOG(LVL_trace, "config read from " << filename << " :\n" << (*this));
}

void Config::updateFromCommandLine(
    const boost::program_options::variables_map& cmd) {
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
