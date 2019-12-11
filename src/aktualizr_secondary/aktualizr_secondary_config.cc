#include "aktualizr_secondary_config.h"

#include <sstream>

void AktualizrSecondaryNetConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(port, "port", pt);
  CopyFromConfig(primary_ip, "primary_ip", pt);
  CopyFromConfig(primary_port, "primary_port", pt);
}

void AktualizrSecondaryNetConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, port, "port");
  writeOption(out_stream, primary_ip, "primary_ip");
  writeOption(out_stream, primary_port, "primary_port");
}

void AktualizrSecondaryUptaneConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(ecu_serial, "ecu_serial", pt);
  CopyFromConfig(ecu_hardware_id, "ecu_hardware_id", pt);
  CopyFromConfig(key_source, "key_source", pt);
  CopyFromConfig(key_type, "key_type", pt);
}

void AktualizrSecondaryUptaneConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, ecu_serial, "ecu_serial");
  writeOption(out_stream, ecu_hardware_id, "ecu_hardware_id");
  writeOption(out_stream, key_source, "key_source");
  writeOption(out_stream, key_type, "key_type");
}

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::program_options::variables_map& cmd) {
  // Redundantly check and set the loglevel from the commandline prematurely so
  // that it is taken account while processing the config.
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
    logger_set_threshold(logger);
  }

  if (cmd.count("config") > 0) {
    const auto configs = cmd["config"].as<std::vector<boost::filesystem::path>>();
    checkDirs(configs);
    updateFromDirs(configs);
  } else {
    updateFromDirs(config_dirs_);
  }
  updateFromCommandLine(cmd);
  postUpdateValues();
}

AktualizrSecondaryConfig::AktualizrSecondaryConfig(const boost::filesystem::path& filename) {
  updateFromToml(filename);
  postUpdateValues();
}

KeyManagerConfig AktualizrSecondaryConfig::keymanagerConfig() const {
  // Note: use dummy values for tls key sources
  return KeyManagerConfig{
      p11, CryptoSource::kFile, CryptoSource::kFile, CryptoSource::kFile, uptane.key_type, uptane.key_source};
}

void AktualizrSecondaryConfig::postUpdateValues() {
  logger_set_threshold(logger);
  LOG_TRACE << "Final configuration that will be used: \n" << (*this);
}

void AktualizrSecondaryConfig::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
  }
  if (cmd.count("server-port") != 0) {
    network.port = cmd["server-port"].as<in_port_t>();
  }
  if (cmd.count("ecu-serial") != 0) {
    uptane.ecu_serial = cmd["ecu-serial"].as<std::string>();
  }
  if (cmd.count("ecu-hardware-id") != 0) {
    uptane.ecu_hardware_id = cmd["ecu-hardware-id"].as<std::string>();
  }
}

void AktualizrSecondaryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in aktualizr_secondary_config.h and
  // AktualizrSecondaryConfig::writeToStream().

  // from aktualizr config
  CopySubtreeFromConfig(logger, "logger", pt);
  // If not already set from the commandline, set the loglevel now so that it
  // affects the rest of the config processing.
  logger_set_threshold(logger);

  CopySubtreeFromConfig(network, "network", pt);
  CopySubtreeFromConfig(uptane, "uptane", pt);

  // from aktualizr config
  CopySubtreeFromConfig(p11, "p11", pt);
  CopySubtreeFromConfig(pacman, "pacman", pt);
  CopySubtreeFromConfig(storage, "storage", pt);
  CopySubtreeFromConfig(import, "import", pt);
}

void AktualizrSecondaryConfig::writeToStream(std::ostream& sink) const {
  // Keep this order the same as in aktualizr_secondary_config.h and
  // AktualizrSecondaryConfig::updateFromPropertyTree().
  WriteSectionToStream(logger, "logger", sink);
  WriteSectionToStream(network, "network", sink);
  WriteSectionToStream(uptane, "uptane", sink);

  WriteSectionToStream(p11, "p11", sink);
  WriteSectionToStream(pacman, "pacman", sink);
  WriteSectionToStream(storage, "storage", sink);
  WriteSectionToStream(import, "import", sink);
}

std::ostream& operator<<(std::ostream& os, const AktualizrSecondaryConfig& cfg) {
  cfg.writeToStream(os);
  return os;
}
