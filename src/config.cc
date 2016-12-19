#include "config.h"

#include <boost/property_tree/ini_parser.hpp>

#include "logger.h"

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
            << "\tВevice UUID: " << cfg.device.uuid;
}


void Config::updateFromToml(const std::string& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename, pt);

  // CoreConfig
  try {
    core.server = pt.get<std::string>("core.server");
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
    auth.server = pt.get<std::string>("auth.server");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.server' option have been found in config file: "
                   << filename << ", Falling back to default: " << auth.server);
  }

  try {
    auth.client_id = pt.get<std::string>("auth.client_id");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.client_id' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << auth.client_id);
  }

  try {
    auth.client_secret = pt.get<std::string>("auth.client_secret");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'auth.client_secret' option have been found in config file: "
                   << filename
                   << ", Falling back to default: " << auth.client_secret);
  }

  // DeviceConfig
  try {
    device.uuid = pt.get<std::string>("device.uuid");
  } catch (...) {
    LOGGER_LOG(LVL_debug,
               "no 'device.uuid' option have been found in config file: "
                   << filename << ", Falling back to default: " << device.uuid);
  }

  LOGGER_LOG(LVL_trace, "config read from " << filename << " :\n" << (*this));
}