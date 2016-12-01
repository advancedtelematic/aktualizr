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
 *       boost::log::sources::severity_logger< boost::log::trivial::severity_level >
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


#ifndef LOG_HPP_
#define LOG_HPP_

/*****************************************************************************/
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

/*****************************************************************************/

/**
 * \par Description:
 *    Initialize the boost logging framework. The functions set common
 *    attributes, configures stderr to be the default sink and sets
 *    an initial logging severity. The message format is set to:
 *    "[%TimeStamp%]: %Message%"
 *
 */
extern void logger_init(void);

/**
 * \par Description:
 *    Sets the global severity level of the boost logging framework.
 *
 * \param[in] sev severity level, use the enumeration from
 *                boost::log::trivial
 */
extern void logger_setSeverity(boost::log::trivial::severity_level lvl);

/**
 * \par Description:
 *    Get the currently set severity level of the logging framework.
 *
 * \return The currently used severity level
 */
extern boost::log::trivial::severity_level logger_getSeverity(void);


#endif // LOG_HPP_
