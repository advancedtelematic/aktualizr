/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file logger.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Refer to the modules header file logger.hpp.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */

#include <iostream>
#include <string>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>


#include "logger.hpp"

/*****************************************************************************/
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace logtrv = boost::log::trivial;

/*****************************************************************************/

static loggerLevels_t logger_globalLvl; /**< store the currently used severity level */

/*****************************************************************************/

// refer to boost::log::trivial::severity_level enumeration
#define LOG_INIT_LEVEL LVL_warning  /**< default logging level used for initialization */

/*****************************************************************************/
void logger_init(void)
{

   // choose stderr as logging console
   logging::add_console_log(std::cerr,
         keywords::format = "[%TimeStamp%]: %Message%");

   // apply default attributes
   logging::add_common_attributes();

   // set the initial severity level
   logger_setSeverity(LOG_INIT_LEVEL);
}

/*****************************************************************************/
void logger_setSeverity(loggerLevels_t lvl)
{
   // set global logging level by applying an corresponding filter

   if (lvl <= LVL_error)
   {
      logging::core::get()->set_filter
            (
                  logtrv::severity >= static_cast<logtrv::severity_level>(lvl)
            );

      // store the currently used log level
      logger_globalLvl = lvl;

      // write an log message
      LOGGER_LOG(LVL_debug, "loglevel set to " << static_cast<logtrv::severity_level>(logger_globalLvl));
   }
   else
   {
      LOGGER_LOG(LVL_debug, "Invalid log level received: " << lvl);
   }
}

/*****************************************************************************/
loggerLevels_t logger_getSeverity(void)
{
   return logger_globalLvl;
}

/*****************************************************************************/
void logger_writeMessage(loggerLevels_t lvl, const std::string& message)
{
   // create a log object as required by the boost logging framework
   static boost::log::sources::severity_logger< boost::log::trivial::severity_level > logger;
   // write a message using the log object and the provided log level
   BOOST_LOG_SEV(logger, static_cast<logtrv::severity_level>(lvl)) << message;
}

// EOF
