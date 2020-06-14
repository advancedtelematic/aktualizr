#include <boost/property_tree/ini_parser.hpp>

#include <libaktualizr/logging_config.h>
#include <libaktualizr/config_utils.h>

void LoggerConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(loglevel, "loglevel", pt);
}

void LoggerConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, loglevel, "loglevel"); }
