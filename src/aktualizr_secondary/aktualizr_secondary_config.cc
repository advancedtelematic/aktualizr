#include "aktualizr_secondary_config.h"

#include <sstream>

#include "config_utils.h"

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::filesystem::path& filename,
                                                   const boost::program_options::variables_map& cmd) {
  updateFromToml(filename);
  updateFromCommandLine(cmd);
}

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::filesystem::path& filename) {
  updateFromToml(filename);
}

void AktualizrSecondaryConfig::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("server-port") != 0) {
    network.port = cmd["server-port"].as<int>();
  }
}

void AktualizrSecondaryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in secondary_config.h and writeToFile().
  CopyFromConfig(network.port, "network.port", boost::log::trivial::info, pt);
}

std::ostream& operator<<(std::ostream& os, const AktualizrSecondaryConfig& cfg) {
  return os << "\tServer listening port: " << cfg.network.port << std::endl;
}

void AktualizrSecondaryConfig::updateFromToml(const boost::filesystem::path& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename.string(), pt);
  updateFromPropertyTree(pt);
  LOG_TRACE << "config read from " << filename << " :\n" << (*this);
}

void AktualizrSecondaryConfig::writeToFile(const boost::filesystem::path& filename) {
  // Keep this order the same as in secondary_config.h and updateFromPropertyTree().
  std::ofstream sink(filename.c_str(), std::ofstream::out);
  sink << std::boolalpha;

  sink << "[network]\n";
  writeOption(sink, network.port, "port");
  sink << "\n";
}
