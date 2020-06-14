#ifndef LOGGING_CONFIG_H
#define LOGGING_CONFIG_H

#include <boost/property_tree/ptree_fwd.hpp>
#include <ostream>

struct LoggerConfig {
  int loglevel{2};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // LOGGING_CONFIG
