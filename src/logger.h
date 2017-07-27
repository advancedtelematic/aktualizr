/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file logger.hpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Simplify usage of the boost logging framework
 *
 * \par Purpose
 *      This module provides a simple interface to the boost logging framework.
 *      In addition to a simple initialization and severity level change, all
 *      boost headers required for writing log messages are included in this
 *      file.
 *
 * \note To write a log message, create an object of type
 *       boost::log::sources::severity_logger<
 *boost::log::trivial::severity_level >
 *       and use the Macro
 *       BOOST_LOG_SEV(#logobject#, boost::log::trivial::#level#) << #message#;
 *
 * \warning Before loggoer_init() is called, the log messages are written to
 *          stdout by default.
 *
 ******************************************************************************
 *
 * \endcond
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <sstream>

/*****************************************************************************/
typedef enum {
  LVL_trace = 0,
  LVL_minimum = LVL_trace,
  LVL_debug,
  LVL_info,
  LVL_warning,
  LVL_error,
  LVL_maximum = LVL_error
} LoggerLevels; /**< define own logging levels */

/*****************************************************************************/
/**
 * \par Description:
 *    Function like macro to pass messages to the logger using a stringstream
 *    object in between.
 *
 * \param[in] _lvl - the log message level using loggerLevels_t
 * \param[in] _stream - the message, put anything here that fits into a
 *                      stringstream
 */
#define LOGGER_LOG(_lvl, _stream)              \
  {                                            \
    std::stringstream logstring;               \
    logstring << _stream;                      \
    loggerWriteMessage(_lvl, logstring.str()); \
  }

/*****************************************************************************/
/**
 * \par Description:
 *    Initialize the boost logging framework. The functions set common
 *    attributes, configures stderr to be the default sink and sets
 *    an initial logging severity. The message format is set to:
 *    "[%TimeStamp%]: %Message%"
 *
 */
extern void loggerInit(void);

/**
 * \par Description:
 *    Sets the global severity level of the boost logging framework.
 *
 * \param[in] sev severity level, use the enumeration from
 *                boost::log::trivial
 */
extern void loggerSetSeverity(LoggerLevels lvl);

/**
 * \par Description:
 *    Get the currently set severity level of the logging framework.
 *
 * \return The currently used severity level
 */
extern LoggerLevels loggerGetSeverity(void);

/**
 * \par Description:
 *    Writes a log message using the configured logging framework.
 *
 * \param[in] lvl - the log or severity level of the message
 * \param[in] message - the message provided as constant string reference
 */
extern void loggerWriteMessage(LoggerLevels lvl, const std::string& message);

#endif  // LOGGER_H_
