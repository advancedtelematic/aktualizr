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

#include <iostream>
#include <string>
#include <iosfwd>

#include <yaml-cpp/yaml.h>

#include "logger.hpp"
#include "ymlcfg.hpp"

/*****************************************************************************/
typedef struct{
   std::string url;
   std::string client_id;
   std::string secret;
   unsigned int loglevel;
}config_data_t; /**< list of known configuration data */

/*****************************************************************************/
static config_data_t config_data;   /**< store configuration data locally until the future destinations are implemented */

/*****************************************************************************/
/**
 * \par Description:
 *    Overload the << operator for the configuration data structure allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const config_data_t& cfg) {
    return os << "Server: " << cfg.url << std::endl
              << "Client: " << cfg.client_id << std::endl
              << "Secret: " << cfg.secret << std::endl
              << "Loglevel: " << cfg.loglevel << std::endl;
}


/*****************************************************************************/
void ymlcfg_readFile(const std::string& filename)
{
   // load the configuration file
   YAML::Node ymlconfig = YAML::LoadFile(filename);

   // check for keys in the configuration file
   if (ymlconfig["server"]) {
     config_data.url = ymlconfig["server"].as<std::string>();
   }
   else
   {
      std::cout << "no key server \n";
   }

   if (ymlconfig["client_id"]) {
     config_data.client_id = ymlconfig["client_id"].as<std::string>();
   }
   else
   {
      std::cout << "no key client_id \n";
   }

   if (ymlconfig["client_secret"]) {
     config_data.secret = ymlconfig["client_secret"].as<std::string>();
   }
   else
   {
      std::cout << "no key client_secret \n";
   }

   if (ymlconfig["loglevel"]) {
     config_data.loglevel = ymlconfig["loglevel"].as<unsigned int>();
     logger_setSeverity(static_cast<loggerLevels_t>(config_data.loglevel));
   }
   else
   {
      std::cout << "no key loglevel \n";
   }

   LOGGER_LOG(LVL_debug, "config read from " << filename << " : " << config_data);

}
