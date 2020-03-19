#include <iomanip>
#include <iostream>

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>

static void color_fmt(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
  auto severity = rec[boost::log::trivial::severity];
  bool color = false;
  if (severity) {
    switch (severity.get()) {
      case boost::log::trivial::warning:
        strm << "\033[33m";
        color = true;
        break;
      case boost::log::trivial::error:
      case boost::log::trivial::fatal:
        strm << "\033[31m";
        color = true;
        break;
      default:
        break;
    }
  }
  // setw(7) = the longest log level. eg "warning"
  strm << std::setw(7) << std::setfill(' ') << severity << ": " << rec[boost::log::expressions::smessage];
  if (color) {
    strm << "\033[0m";
  }
}

void logger_init_sink(bool use_colors = false) {
  auto* stream = &std::cerr;
  if (getenv("LOG_STDERR") == nullptr) {
    stream = &std::cout;
  }
  auto sink = boost::log::add_console_log(*stream, boost::log::keywords::format = "%Message%",
                                          boost::log::keywords::auto_flush = true);
  if (use_colors) {
    sink->set_formatter(&color_fmt);
  }
}
