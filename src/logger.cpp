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


#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "log.hpp"

/*****************************************************************************/
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace logtrv = boost::log::trivial;

/*****************************************************************************/
static logtrv::severity_level logger_globalLvl; /**< store the currently used severity level */

/*****************************************************************************/

// refer to boost::log::trivial::severity_level enumeration
#define LOG_INIT_LEVEL logtrv::warning  /**< default logging level used for initialization */

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
void logger_setSeverity(logtrv::severity_level lvl)
{
   // set global logging level by applying an corresponding filter
   logging::core::get()->set_filter
         (
               logtrv::severity >= lvl
         );
   logger_globalLvl = lvl;

}

/*****************************************************************************/
logtrv::severity_level logger_getSeverity(void)
{
   return logger_globalLvl;
}
