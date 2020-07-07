#ifndef SOTA_CLIENT_TOOLS_LOGGING_H_
#define SOTA_CLIENT_TOOLS_LOGGING_H_

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

struct LoggerConfig;

/** Log an unrecoverable error */
#define LOG_FATAL BOOST_LOG_TRIVIAL(fatal)

/** Log that something has definitely gone wrong */
#define LOG_ERROR BOOST_LOG_TRIVIAL(error)

/** Warn about behaviour that is probably bad, but hasn't yet caused the system
 * to operate out of spec. */
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)

/** Report a user-visible message about operation */
#define LOG_INFO BOOST_LOG_TRIVIAL(info)

/** Report a message for developer debugging */
#define LOG_DEBUG BOOST_LOG_TRIVIAL(debug)

/** Report very-verbose debugging information */
#define LOG_TRACE BOOST_LOG_TRIVIAL(trace)

// Use like:
// curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
int64_t get_curlopt_verbose();

void logger_init(bool use_colors = false);

void logger_set_threshold(boost::log::trivial::severity_level threshold);

void logger_set_threshold(const LoggerConfig& lconfig);

void logger_set_enable(bool enabled);

int loggerGetSeverity();

#endif
