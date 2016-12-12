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
  std::string url;
  std::string client_id;
  std::string secret;
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
  return os << "\tServer: " << cfg.url << std::endl
            << "\tClient: " << cfg.client_id << std::endl
            << "\tSecret: " << cfg.secret << std::endl
            << "\tLoglevel: " << cfg.loglevel;
}

/*****************************************************************************/
void ymlcfg_readFile(const std::string& filename) {
  // load the configuration file
  YAML::Node ymlconfig = YAML::LoadFile(filename);

  // check for keys in the configuration file
  if (ymlconfig["server"]) {
    config_data.url = ymlconfig["server"].as<std::string>();
  } else {
    LOGGER_LOG(LVL_debug,
               "ymlcfg - no server URL found in config file: " << filename);
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
               "ymlcfg - no loglecvel found in config file:" << filename);
  }

  LOGGER_LOG(LVL_debug, "ymlcfg - config read from " << filename << " :\n"
                                                     << config_data);
}

/*****************************************************************************/
unsigned int ymlcfg_setServerData(sota_server::servercon* sota_serverPtr) {
  unsigned int returnValue;
  returnValue = 0u;

  if (!config_data.url.empty()) {
    if (!config_data.client_id.empty()) {
      if (!config_data.secret.empty()) {
        sota_serverPtr->setServer(config_data.url);
        sota_serverPtr->setClientID(config_data.client_id);
        sota_serverPtr->setClientSecret(config_data.secret);
        returnValue = 1u;
      }
    }
  }

  return returnValue;
}
