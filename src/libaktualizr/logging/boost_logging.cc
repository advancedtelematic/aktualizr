#include "logging.h"

#include <boost/log/utility/setup/console.hpp>

namespace logging = boost::log;
using boost::log::trivial::severity_level;

severity_level gLoggingThreshold;

int64_t get_curlopt_verbose() { return gLoggingThreshold <= boost::log::trivial::trace ? 1L : 0L; }

void logger_init() {
  gLoggingThreshold = boost::log::trivial::info;
  logging::add_console_log(std::cout, boost::log::keywords::format = "%Message%",
                           boost::log::keywords::auto_flush = true);
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= gLoggingThreshold);
}

void logger_set_threshold(severity_level threshold) {
  gLoggingThreshold = threshold;
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= gLoggingThreshold);
}

int loggerGetSeverity() { return static_cast<int>(gLoggingThreshold); }

void LoggerConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(loglevel, "loglevel", boost::log::trivial::trace, pt);
}

void LoggerConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, loglevel, "loglevel"); }

// vim: set tabstop=2 shiftwidth=2 expandtab:
