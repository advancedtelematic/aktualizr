#include "aktualizr_secondary_config.h"

#include <sstream>

#include "config_utils.h"

void AktualizrSecondaryNetConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(port, "port", boost::log::trivial::info, pt);
  CopyFromConfig(discovery, "discovery", boost::log::trivial::info, pt);
  CopyFromConfig(discovery_port, "discovery_port", boost::log::trivial::info, pt);
}

void AktualizrSecondaryNetConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, port, "port");
  writeOption(out_stream, discovery, "discovery");
  writeOption(out_stream, discovery_port, "discovery_port");
}

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
  CopySubtreeFromConfig(network, "network", pt);

  // from aktualizr config
  CopySubtreeFromConfig(storage, "storage", pt);
  CopySubtreeFromConfig(p11, "p11", pt);
  CopySubtreeFromConfig(uptane, "uptane", pt);
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

  WriteSectionToStream(network, "network", sink);

  // from aktualizr config
  WriteSectionToStream(storage, "storage", sink);
  WriteSectionToStream(p11, "p11", sink);
  WriteSectionToStream(uptane, "uptane", sink);
}
