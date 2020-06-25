#include <libaktualizr/config.h>

#include "logging.h"

using boost::log::trivial::severity_level;

static severity_level gLoggingThreshold;

extern void logger_init_sink(bool use_colors = false);

int64_t get_curlopt_verbose() { return gLoggingThreshold <= boost::log::trivial::trace ? 1L : 0L; }

void logger_init(bool use_colors) {
  gLoggingThreshold = boost::log::trivial::info;

  logger_init_sink(use_colors);

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

void logger_set_enable(bool enabled) { boost::log::core::get()->set_logging_enabled(enabled); }

int loggerGetSeverity() { return static_cast<int>(gLoggingThreshold); }

// vim: set tabstop=2 shiftwidth=2 expandtab:
