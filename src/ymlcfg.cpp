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

#include <iosfwd>
#include <iostream>
#include <string>

#include <yaml-cpp/yaml.h>

#include "logger.hpp"
#include "servercon.hpp"
#include "ymlcfg.hpp"

/*****************************************************************************/
typedef struct {
  std::string authurl;
  std::string sotaurl;
  std::string client_id;
  std::string secret;
  std::string devUUID;
  unsigned int loglevel;
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
  return os << "\tAuthtentification Server: " << cfg.authurl << std::endl
            << "\tSota Server: " << cfg.sotaurl << std::endl
            << "\tClient: " << cfg.client_id << std::endl
            << "\tSecret: " << cfg.secret << std::endl
            << "\tLoglevel: " << cfg.loglevel << std::endl
            << "\t device UUID: " << cfg.devUUID;
}

/*****************************************************************************/
void ymlcfg_readFile(const std::string& filename) {
  // load the configuration file
  YAML::Node ymlconfig = YAML::LoadFile(filename);

  // check for keys in the configuration file
  if (ymlconfig["sotaserver"]) {
    config_data.sotaurl = ymlconfig["sotaserver"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no sotaserver URL found in config file: " << filename);
  }

  // check for keys in the configuration file
  if (ymlconfig["authserver"]) {
    config_data.authurl = ymlconfig["authserver"].as<std::string>();
  }
  // use the sota server for authentication if no separate authentication
  // server is provided.
  else if (ymlconfig["sotaserver"]) {
    config_data.authurl = ymlconfig["sotaserver"].as<std::string>();
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
    config_data.loglevel = ymlconfig["loglevel"].as<unsigned int>();

    logger_setSeverity(static_cast<loggerLevels_t>(config_data.loglevel));
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no loglevel found in config file:" << filename);
  }

  if (ymlconfig["devUUID"]) {
    config_data.devUUID = ymlconfig["devUUID"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no devUUID found in config file:" << filename);
  }

  LOGGER_LOG(LVL_debug, "ymlcfg - config read from " << filename << " :\n"
                                                     << config_data);
}

/*****************************************************************************/
unsigned int ymlcfg_setServerData(sota_server::servercon* sota_serverPtr) {
  unsigned int returnValue;
  returnValue = 0u;

  // check if required configuration data is available
  if ((!config_data.authurl.empty()) && (!config_data.client_id.empty()) &&
      (!config_data.secret.empty()) && (!config_data.devUUID.empty()) &&
      (!config_data.sotaurl.empty())) {
    sota_serverPtr->setAuthServer(
        const_cast<std::string&>(config_data.authurl));
    sota_serverPtr->setClientID(
        const_cast<std::string&>(config_data.client_id));
    sota_serverPtr->setClientSecret(
        const_cast<std::string&>(config_data.secret));
    sota_serverPtr->setDevUUID(const_cast<std::string&>(config_data.devUUID));
    sota_serverPtr->setSotaServer(
        const_cast<std::string&>(config_data.sotaurl));

    returnValue = 1u;
  }

  return returnValue;
}
