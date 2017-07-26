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

#include "logger.h"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <iostream>
#include <string>

/*****************************************************************************/
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace logtrv = boost::log::trivial;

/*****************************************************************************/

static LoggerLevels logger_global_level; /**< store the currently used severity level */

/*****************************************************************************/
void loggerInit(void) {
  // choose stderr as logging console
  logging::add_console_log(std::cout, keywords::format = "[%TimeStamp%]: %Message%");

  // apply default attributes
  logging::add_common_attributes();
}

/*****************************************************************************/
void loggerSetSeverity(LoggerLevels lvl) {
  // set global logging level by applying an corresponding filter

  if (lvl <= LVL_error) {
    logging::core::get()->set_filter(logtrv::severity >= static_cast<logtrv::severity_level>(lvl));

    // store the currently used log level
    logger_global_level = lvl;

    // write an log message
    LOGGER_LOG(LVL_debug, "loglevel set to " << static_cast<logtrv::severity_level>(logger_global_level));
  } else {
    LOGGER_LOG(LVL_debug, "Invalid log level received: " << lvl);
  }
}

/*****************************************************************************/
LoggerLevels loggerGetSeverity(void) { return logger_global_level; }

/*****************************************************************************/
void loggerWriteMessage(LoggerLevels lvl, const std::string& message) {
  // create a log object as required by the boost logging framework
  static boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
  // write a message using the log object and the provided log level
  BOOST_LOG_SEV(logger, static_cast<logtrv::severity_level>(lvl)) << message;
}

// EOF
