/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file ymlcfg.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Refer to the modules header file ymlcfg.hpp.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */

#include "ymlcfg.h"

#include <iosfwd>
#include <iostream>
#include <string>
#include <yaml-cpp/yaml.h>

#include "logger.h"
#include "servercon.h"

/*****************************************************************************/
typedef struct {
  std::string auth_url;
  std::string sota_url;
  std::string client_id;
  std::string secret;
  std::string device_UUID;
  unsigned int log_level;
} config_data_t; /**< list of known configuration data */

/*****************************************************************************/
static config_data_t config_data; /**< store configuration data locally until
                                     the future destinations are implemented */

/*****************************************************************************/
/**
 * \par Description:
 *    Overload the << operator for the configuration data structure allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const config_data_t& cfg) {
  return os << "\tAuthtentification Server: " << cfg.auth_url << std::endl
            << "\tSota Server: " << cfg.sota_url << std::endl
            << "\tClient: " << cfg.client_id << std::endl
            << "\tSecret: " << cfg.secret << std::endl
            << "\tLoglevel: " << cfg.log_level << std::endl
            << "\t device UUID: " << cfg.device_UUID;
}

/*****************************************************************************/
void ymlcfgReadFile(const std::string& filename) {
  // load the configuration file
  YAML::Node ymlconfig = YAML::LoadFile(filename);

  // check for keys in the configuration file
  if (ymlconfig["sotaserver"]) {
    config_data.sota_url = ymlconfig["sotaserver"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no sotaserver URL found in config file: " << filename);
  }

  // check for keys in the configuration file
  if (ymlconfig["authserver"]) {
    config_data.auth_url = ymlconfig["authserver"].as<std::string>();
  }
  // use the sota server for authentication if no separate authentication
  // server is provided.
  else if (ymlconfig["sotaserver"]) {
    config_data.auth_url = ymlconfig["sotaserver"].as<std::string>();
    LOGGER_LOG(LVL_debug, "ymlcfg - using SOTA server for OAuth2 authentication"
                              << "as no authentication server was provided in "
                              << filename);
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no authserver URL found in config file: " << filename);
  }

  if (ymlconfig["client_id"]) {
    config_data.client_id = ymlconfig["client_id"].as<std::string>();

  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no clientID found in config file: " << filename);
  }

  if (ymlconfig["client_secret"]) {
    config_data.secret = ymlconfig["client_secret"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no client-secret found in config file: " << filename);
  }

  if (ymlconfig["loglevel"]) {
    config_data.log_level = ymlconfig["loglevel"].as<unsigned int>();

    loggerSetSeverity(static_cast<LoggerLevels>(config_data.log_level));
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no loglevel found in config file:" << filename);
  }

  if (ymlconfig["devUUID"]) {
    config_data.device_UUID = ymlconfig["devUUID"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no devUUID found in config file:" << filename);
  }

  LOGGER_LOG(LVL_debug, "ymlcfg - config read from " << filename << " :\n"
                                                     << config_data);
}

/*****************************************************************************/
unsigned int ymlcfgSetServerData(sota_server::ServerCon* sota_serverPtr) {
  unsigned int return_value = 0u;

  // check if required configuration data is available
  if ((!config_data.auth_url.empty()) && (!config_data.client_id.empty()) &&
      (!config_data.secret.empty()) && (!config_data.device_UUID.empty()) &&
      (!config_data.sota_url.empty())) {
    sota_serverPtr->setAuthServer(
        const_cast<std::string&>(config_data.auth_url));
    sota_serverPtr->setClientID(
        const_cast<std::string&>(config_data.client_id));
    sota_serverPtr->setClientSecret(
        const_cast<std::string&>(config_data.secret));
    sota_serverPtr->setDevUUID(
        const_cast<std::string&>(config_data.device_UUID));
    sota_serverPtr->setSotaServer(
        const_cast<std::string&>(config_data.sota_url));

    return_value = 1u;
  }

  return return_value;
}
