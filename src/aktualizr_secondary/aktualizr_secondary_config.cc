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
  if (cmd.count("discovery-port") != 0) {
    int p = cmd["discovery_port"].as<int>();
    if (p == 0) {
      network.discovery = false;
    } else {
      network.discovery = true;
    }
    network.discovery_port = p;
  }
}

void AktualizrSecondaryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in secondary_config.h and writeToFile().
  CopyFromConfig(network.port, "network.port", boost::log::trivial::info, pt);
  CopyFromConfig(network.discovery, "network.discovery", boost::log::trivial::info, pt);
  CopyFromConfig(network.discovery_port, "network.discovery_port", boost::log::trivial::info, pt);

  // from aktualizr config
  CopySubtreeFromConfig(storage, "storage", pt);
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
  writeOption(sink, network.discovery, "discovery");
  writeOption(sink, network.discovery_port, "discovery_port");
  sink << "\n";

  WriteSectionToStream(storage, "storage", sink);
}
