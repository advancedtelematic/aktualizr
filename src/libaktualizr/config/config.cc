#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>

#include "bootstrap/bootstrap.h"
#include "config.h"
#include "utilities/exceptions.h"
#include "utilities/utils.h"

void NetworkConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(socket_commands_path, "socket_commands_path", pt);
  CopyFromConfig(socket_events_path, "socket_events_path", pt);

  // TODO: fix this to support multiple config files (if we care):
  boost::optional<std::string> events_string = pt.get_optional<std::string>("socket_events");
  if (events_string.is_initialized()) {
    std::string e = Utils::stripQuotes(events_string.get());
    socket_events.empty();
    boost::split(socket_events, e, boost::is_any_of(", "), boost::token_compress_on);
  }

  CopyFromConfig(ipdiscovery_host, "ipdiscovery_host", pt);
  CopyFromConfig(ipdiscovery_port, "ipdiscovery_port", pt);
  CopyFromConfig(ipdiscovery_wait_seconds, "ipdiscovery_wait_seconds", pt);
  CopyFromConfig(ipuptane_port, "ipuptane_port", pt);
}

void NetworkConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, socket_commands_path, "socket_commands_path");
  writeOption(out_stream, socket_events_path, "socket_events_path");
  // TODO: fix this to support multiple config files (if we care):
  std::string events_str;
  for (auto it = socket_events.begin(); it != socket_events.end(); ++it) {
    events_str += *it;
    if (it != --socket_events.end()) {
      events_str += ", ";
    }
  }
  writeOption(out_stream, events_str, "socket_events");
  writeOption(out_stream, ipdiscovery_host, "ipdiscovery_host");
  writeOption(out_stream, ipdiscovery_port, "ipdiscovery_port");
  writeOption(out_stream, ipdiscovery_wait_seconds, "ipdiscovery_wait_seconds");
  writeOption(out_stream, ipuptane_port, "ipuptane_port");
}

void TlsConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(server, "server", pt);
  CopyFromConfig(server_url_path, "server_url_path", pt);
  CopyFromConfig(ca_source, "ca_source", pt);
  CopyFromConfig(cert_source, "cert_source", pt);
  CopyFromConfig(pkey_source, "pkey_source", pt);
}

void TlsConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, server, "server");
  writeOption(out_stream, server_url_path, "server_url_path");
  writeOption(out_stream, ca_source, "ca_source");
  writeOption(out_stream, pkey_source, "pkey_source");
  writeOption(out_stream, cert_source, "cert_source");
}

void ProvisionConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(server, "server", pt);
  CopyFromConfig(p12_password, "p12_password", pt);
  CopyFromConfig(expiry_days, "expiry_days", pt);
  CopyFromConfig(provision_path, "provision_path", pt);
  CopyFromConfig(device_id, "device_id", pt);
  CopyFromConfig(primary_ecu_serial, "primary_ecu_serial", pt);
  CopyFromConfig(primary_ecu_hardware_id, "primary_ecu_hardware_id", pt);
  CopyFromConfig(ecu_registration_endpoint, "ecu_registration_endpoint", pt);
  // provision.mode is set in postUpdateValues.
}

void ProvisionConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, server, "server");
  writeOption(out_stream, p12_password, "p12_password");
  writeOption(out_stream, expiry_days, "expiry_days");
  writeOption(out_stream, provision_path, "provision_path");
  writeOption(out_stream, device_id, "device_id");
  writeOption(out_stream, primary_ecu_serial, "primary_ecu_serial");
  writeOption(out_stream, primary_ecu_hardware_id, "primary_ecu_hardware_id");
  writeOption(out_stream, ecu_registration_endpoint, "ecu_registration_endpoint");
  // Skip provision.mode since it is dependent on other options.
}

void UptaneConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(running_mode, "running_mode", pt);
  CopyFromConfig(polling_sec, "polling_sec", pt);
  CopyFromConfig(director_server, "director_server", pt);
  CopyFromConfig(repo_server, "repo_server", pt);
  CopyFromConfig(key_source, "key_source", pt);
  CopyFromConfig(key_type, "key_type", pt);
  CopyFromConfig(legacy_interface, "legacy_interface", pt);
  // uptane.secondary_configs is populated by processing secondary configs from
  // the commandline and uptane.legacy_interface.
}

void UptaneConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, StringFromRunningMode(running_mode), "running_mode");
  writeOption(out_stream, polling_sec, "polling_sec");
  writeOption(out_stream, director_server, "director_server");
  writeOption(out_stream, repo_server, "repo_server");
  writeOption(out_stream, key_source, "key_source");
  writeOption(out_stream, key_type, "key_type");
  writeOption(out_stream, legacy_interface, "legacy_interface");
}

void DiscoveryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(ipuptane, "ipuptane", pt);
}

void DiscoveryConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, ipuptane, "ipuptane"); }

/**
 * \par Description:
 *    Overload the << operator for the configuration class allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const Config& cfg) {
  cfg.writeToStream(os);
  return os;
}

Config::Config() { postUpdateValues(); }

Config::Config(const boost::filesystem::path& filename) {
  updateFromToml(filename);
  postUpdateValues();
}

Config::Config(const std::vector<boost::filesystem::path>& config_dirs) {
  checkDirs(config_dirs);
  updateFromDirs(config_dirs);
  postUpdateValues();
}

Config::Config(const boost::program_options::variables_map& cmd) {
  // Redundantly check and set the loglevel from the commandline prematurely so
  // that it is taken account while processing the config.
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
    logger_set_threshold(logger);
    loglevel_from_cmdline = true;
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

KeyManagerConfig Config::keymanagerConfig() const {
  return KeyManagerConfig{p11, tls.ca_source, tls.pkey_source, tls.cert_source, uptane.key_type, uptane.key_source};
}

void Config::postUpdateValues() {
  logger_set_threshold(logger);

  if (provision.provision_path.empty()) {
    provision.mode = ProvisionMode::kImplicit;
  }

  if (tls.server.empty()) {
    if (!tls.server_url_path.empty()) {
      try {
        tls.server = Utils::readFile(tls.server_url_path);
      } catch (const boost::filesystem::filesystem_error& e) {
        LOG_ERROR << "Couldn't read gateway URL: " << e.what();
        tls.server = "";
      }
    } else if (!provision.provision_path.empty()) {
      if (boost::filesystem::exists(provision.provision_path)) {
        tls.server = Bootstrap::readServerUrl(provision.provision_path);
      } else {
        LOG_ERROR << "Provided provision archive " << provision.provision_path << " does not exist!";
      }
    }
  }

  if (!tls.server.empty()) {
    if (provision.server.empty()) {
      provision.server = tls.server;
    }

    if (uptane.repo_server.empty()) {
      uptane.repo_server = tls.server + "/repo";
    }

    if (uptane.director_server.empty()) {
      uptane.director_server = tls.server + "/director";
    }

    if (pacman.ostree_server.empty()) {
      pacman.ostree_server = tls.server + "/treehub";
    }
  }

  if (!uptane.director_server.empty()) {
    if (provision.ecu_registration_endpoint.empty()) {
      provision.ecu_registration_endpoint = uptane.director_server + "/ecus";
    }
  }

  checkLegacyVersion();
  initLegacySecondaries();
  LOG_TRACE << "Final configuration that will be used: \n" << (*this);
}

// For testing
void Config::updateFromTomlString(const std::string& contents) {
  boost::property_tree::ptree pt;
  std::stringstream stream(contents);
  boost::property_tree::ini_parser::read_ini(stream, pt);
  updateFromPropertyTree(pt);
}

void Config::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in config.h and Config::writeToStream().
  if (!loglevel_from_cmdline) {
    CopySubtreeFromConfig(logger, "logger", pt);
    // If not already set from the commandline, set the loglevel now so that it
    // affects the rest of the config processing.
    logger_set_threshold(logger);
  }
  CopySubtreeFromConfig(network, "network", pt);
  CopySubtreeFromConfig(p11, "p11", pt);
  CopySubtreeFromConfig(tls, "tls", pt);
  CopySubtreeFromConfig(provision, "provision", pt);
  CopySubtreeFromConfig(uptane, "uptane", pt);
  CopySubtreeFromConfig(discovery, "discovery", pt);
  CopySubtreeFromConfig(pacman, "pacman", pt);
  CopySubtreeFromConfig(storage, "storage", pt);
  CopySubtreeFromConfig(import, "import", pt);
  CopySubtreeFromConfig(telemetry, "telemetry", pt);
  CopySubtreeFromConfig(bootloader, "bootloader", pt);
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  // Try to keep these options in the same order as parse_options() in main.cc.
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
  }
  if (cmd.count("running-mode") != 0) {
    uptane.running_mode = RunningModeFromString(cmd["running-mode"].as<std::string>());
  }
  if (cmd.count("tls-server") != 0) {
    tls.server = cmd["tls-server"].as<std::string>();
  }
  if (cmd.count("repo-server") != 0) {
    uptane.repo_server = cmd["repo-server"].as<std::string>();
  }
  if (cmd.count("director-server") != 0) {
    uptane.director_server = cmd["director-server"].as<std::string>();
  }
  if (cmd.count("ostree-server") != 0) {
    pacman.ostree_server = cmd["ostree-server"].as<std::string>();
  }
  if (cmd.count("primary-ecu-serial") != 0) {
    provision.primary_ecu_serial = cmd["primary-ecu-serial"].as<std::string>();
  }
  if (cmd.count("primary-ecu-hardware-id") != 0) {
    provision.primary_ecu_hardware_id = cmd["primary-ecu-hardware-id"].as<std::string>();
  }
  if (cmd.count("secondary-config") != 0) {
    std::vector<boost::filesystem::path> sconfigs = cmd["secondary-config"].as<std::vector<boost::filesystem::path>>();
    readSecondaryConfigs(sconfigs);
  }
  if (cmd.count("legacy-interface") != 0) {
    uptane.legacy_interface = cmd["legacy-interface"].as<boost::filesystem::path>();
  }
}

void Config::readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs) {
  for (auto it = sconfigs.cbegin(); it != sconfigs.cend(); ++it) {
    uptane.secondary_configs.emplace_back(Uptane::SecondaryConfig(*it));
  }
}

void Config::checkLegacyVersion() {
  if (uptane.legacy_interface.empty()) {
    return;
  }
  if (!boost::filesystem::exists(uptane.legacy_interface)) {
    throw FatalException(std::string("Legacy external flasher not found: ") + uptane.legacy_interface.string());
  }
  std::stringstream command;
  std::string output;
  command << uptane.legacy_interface << " api-version --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    throw FatalException(std::string("Legacy external flasher api-version command failed: ") + output);
  }
  boost::trim_if(output, boost::is_any_of(" \n\r\t"));
  if (output != "1") {
    throw FatalException(std::string("Unexpected legacy external flasher API version: ") + output);
  }
}

void Config::initLegacySecondaries() {
  if (uptane.legacy_interface.empty()) {
    return;
  }
  std::stringstream command;
  std::string output;
  command << uptane.legacy_interface << " list-ecus --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    LOG_ERROR << "Legacy external flasher list-ecus command failed: " << output;
    return;
  }

  std::stringstream ss(output);
  std::string buffer;
  while (std::getline(ss, buffer, '\n')) {
    Uptane::SecondaryConfig sconfig;
    sconfig.secondary_type = Uptane::SecondaryType::kLegacy;
    std::vector<std::string> ecu_info;
    boost::split(ecu_info, buffer, boost::is_any_of(" \n\r\t"), boost::token_compress_on);
    if (ecu_info.size() == 0) {
      // Could print a warning but why bother.
      continue;
    }
    if (ecu_info.size() == 1) {
      sconfig.ecu_hardware_id = ecu_info[0];
      // Use getSerial, which will get the public_key_id, initialized in ManagedSecondary constructor.
      sconfig.ecu_serial = "";
      sconfig.full_client_dir = storage.path / sconfig.ecu_hardware_id;
      LOG_INFO << "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id;
    } else if (ecu_info.size() >= 2) {
      // For now, silently ignore anything after the second token.
      sconfig.ecu_hardware_id = ecu_info[0];
      sconfig.ecu_serial = ecu_info[1];
      sconfig.full_client_dir = storage.path / (sconfig.ecu_hardware_id + "-" + sconfig.ecu_serial);
      LOG_INFO << "Legacy ECU configured with hardware ID " << sconfig.ecu_hardware_id << " and serial "
               << sconfig.ecu_serial;
    }

    sconfig.partial_verifying = false;
    sconfig.ecu_private_key = "sec.private";
    sconfig.ecu_public_key = "sec.public";

    sconfig.firmware_path = sconfig.full_client_dir / "firmware.bin";
    sconfig.metadata_path = sconfig.full_client_dir / "metadata";
    sconfig.target_name_path = sconfig.full_client_dir / "target_name";
    sconfig.flasher = uptane.legacy_interface;

    uptane.secondary_configs.push_back(sconfig);
  }
}

void Config::writeToStream(std::ostream& sink) const {
  // Keep this order the same as in config.h and
  // Config::updateFromPropertyTree().
  WriteSectionToStream(logger, "logger", sink);
  WriteSectionToStream(network, "network", sink);
  WriteSectionToStream(p11, "p11", sink);
  WriteSectionToStream(tls, "tls", sink);
  WriteSectionToStream(provision, "provision", sink);
  WriteSectionToStream(uptane, "uptane", sink);
  WriteSectionToStream(discovery, "discovery", sink);
  WriteSectionToStream(pacman, "pacman", sink);
  WriteSectionToStream(storage, "storage", sink);
  WriteSectionToStream(import, "import", sink);
  WriteSectionToStream(telemetry, "telemetry", sink);
  WriteSectionToStream(bootloader, "bootloader", sink);
}

asn1::Serializer& operator<<(asn1::Serializer& ser, CryptoSource cs) {
  ser << asn1::implicit<kAsn1Enum>(static_cast<const int32_t&>(static_cast<int>(cs)));

  return ser;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const TlsConfig& tls_conf) {
  ser << asn1::seq << asn1::implicit<kAsn1Utf8String>(tls_conf.server)
      << asn1::implicit<kAsn1Utf8String>(tls_conf.server_url_path.string()) << tls_conf.ca_source
      << tls_conf.pkey_source << tls_conf.cert_source << asn1::endseq;
  return ser;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, CryptoSource& cs) {
  int32_t cs_i;
  des >> asn1::implicit<kAsn1Enum>(cs_i);

  if (cs_i < static_cast<int>(CryptoSource::kFile) || cs_i > static_cast<int>(CryptoSource::kPkcs11)) {
    throw deserialization_error();
  }

  cs = static_cast<CryptoSource>(cs_i);

  return des;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, boost::filesystem::path& path) {
  std::string path_string;
  des >> asn1::implicit<kAsn1Utf8String>(path_string);
  path = path_string;
  return des;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, TlsConfig& tls_conf) {
  des >> asn1::seq >> asn1::implicit<kAsn1Utf8String>(tls_conf.server) >> tls_conf.server_url_path >>
      tls_conf.ca_source >> tls_conf.pkey_source >> tls_conf.cert_source >> asn1::endseq;
  return des;
}
