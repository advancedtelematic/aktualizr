#include <iostream>

#include <boost/log/utility/setup/console.hpp>

void logger_init_sink() {
  boost::log::add_console_log(std::cout, boost::log::keywords::format = "%Message%",
                              boost::log::keywords::auto_flush = true);
}
