#include "logging.h"
#include "utilities/config_utils.h"

#include <boost/log/utility/setup/console.hpp>

namespace logging = boost::log;
using boost::log::trivial::severity_level;

static severity_level gLoggingThreshold;

void LoggerConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(loglevel, "loglevel", pt);
}

void LoggerConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, loglevel, "loglevel"); }

int64_t get_curlopt_verbose() { return gLoggingThreshold <= boost::log::trivial::trace ? 1L : 0L; }

void logger_init() {
  gLoggingThreshold = boost::log::trivial::info;
  logging::add_console_log(std::cout, boost::log::keywords::format = "%Message%",
                           boost::log::keywords::auto_flush = true);
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= gLoggingThreshold);
}

void logger_set_threshold(const severity_level threshold) {
  gLoggingThreshold = threshold;
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= gLoggingThreshold);
}

void logger_set_threshold(const LoggerConfig& lconfig) {
  int loglevel = lconfig.loglevel;
  if (loglevel < boost::log::trivial::trace) {
    LOG_WARNING << "Invalid log level: " << loglevel;
    loglevel = boost::log::trivial::trace;
  }
  if (boost::log::trivial::fatal < loglevel) {
    LOG_WARNING << "Invalid log level: " << loglevel;
    loglevel = boost::log::trivial::fatal;
  }
  logger_set_threshold(static_cast<boost::log::trivial::severity_level>(loglevel));
}

int loggerGetSeverity() { return static_cast<int>(gLoggingThreshold); }

// vim: set tabstop=2 shiftwidth=2 expandtab:
