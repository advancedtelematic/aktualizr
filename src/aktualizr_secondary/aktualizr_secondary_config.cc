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

void AktualizrSecondaryUptaneConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(ecu_serial, "ecu_serial", boost::log::trivial::info, pt);
  CopyFromConfig(ecu_hardware_id, "ecu_hardware_id", boost::log::trivial::info, pt);

  // TODO: de-duplicate this from config.cc
  std::string ks = "file";
  CopyFromConfig(ks, "key_source", boost::log::trivial::warning, pt);
  if (ks == "pkcs11")
    key_source = kPkcs11;
  else
    key_source = kFile;

  std::string kt;
  CopyFromConfig(kt, "key_type", boost::log::trivial::warning, pt);
  if (kt.size()) {
    if (kt == "RSA2048") {
      key_type = kRSA2048;
    } else if (kt == "RSA4096") {
      key_type = kRSA4096;
    } else if (kt == "ED25519") {
      key_type = kED25519;
    }
  }
}

void AktualizrSecondaryUptaneConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, ecu_serial, "ecu_serial");
  writeOption(out_stream, ecu_hardware_id, "ecu_hardware_id");
  writeOption(out_stream, key_source, "key_source");
  writeOption(out_stream, key_type, "key_type");
}

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::filesystem::path& filename,
                                                   const boost::program_options::variables_map& cmd) {
  updateFromToml(filename);
  updateFromCommandLine(cmd);
}

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::filesystem::path& filename) {
  updateFromToml(filename);
}

KeyManagerConfig AktualizrSecondaryConfig::keymanagerConfig() const {
  // Note: use dummy values for tls key sources
  return KeyManagerConfig{p11, kFile, kFile, kFile, uptane.key_type, uptane.key_source};
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
  if (cmd.count("ecu-serial") != 0) {
    uptane.ecu_serial = cmd["ecu-serial"].as<std::string>();
  }
  if (cmd.count("ecu-hardware-id") != 0) {
    uptane.ecu_hardware_id = cmd["ecu-hardware-id"].as<std::string>();
  }
}

void AktualizrSecondaryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in secondary_config.h and writeToFile().
  CopySubtreeFromConfig(network, "network", pt);
  CopySubtreeFromConfig(uptane, "uptane", pt);

  // from aktualizr config
  CopySubtreeFromConfig(storage, "storage", pt);
  CopySubtreeFromConfig(p11, "p11", pt);
  CopySubtreeFromConfig(network, "network", pt);
  CopySubtreeFromConfig(pacman, "pacman", pt);
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
  WriteSectionToStream(uptane, "uptane", sink);

  // from aktualizr config
  WriteSectionToStream(storage, "storage", sink);
  WriteSectionToStream(p11, "p11", sink);
  WriteSectionToStream(pacman, "pacman", sink);
}
