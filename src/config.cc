#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>

#include "bootstrap.h"
#include "config.h"
#include "exceptions.h"
#include "utilities/config_utils.h"
#include "utilities/utils.h"

std::ostream& operator<<(std::ostream& os, StorageType stype) {
  std::string stype_str;
  switch (stype) {
    case kFileSystem:
      stype_str = "filesystem";
      break;
    case kSqlite:
      stype_str = "sqlite";
      break;
    default:
      stype_str = "unknown";
      break;
  }
  os << '"' << stype_str << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, KeyType kt) {
  std::string kt_str;
  switch (kt) {
    case kRSA2048:
      kt_str = "RSA2048";
      break;
    case kRSA4096:
      kt_str = "RSA4096";
      break;
    case kED25519:
      kt_str = "ED25519";
      break;
    default:
      kt_str = "unknown";
      break;
  }
  os << '"' << kt_str << '"';
  return os;
}

void GatewayConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(socket, "socket", boost::log::trivial::trace, pt);
}

void GatewayConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, socket, "socket"); }

void NetworkConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(socket_commands_path, "socket_commands_path", boost::log::trivial::trace, pt);
  CopyFromConfig(socket_events_path, "socket_events_path", boost::log::trivial::trace, pt);

  boost::optional<std::string> events_string = pt.get_optional<std::string>("socket_events");
  if (events_string.is_initialized()) {
    std::string e = Utils::stripQuotes(events_string.get());
    socket_events.empty();
    boost::split(socket_events, e, boost::is_any_of(", "), boost::token_compress_on);
  } else {
    LOG_TRACE << "network.socket_events not in config file. Using default";
  }

  CopyFromConfig(ipdiscovery_host, "ipdiscovery_host", boost::log::trivial::trace, pt);
  CopyFromConfig(ipdiscovery_port, "ipdiscovery_port", boost::log::trivial::trace, pt);
  CopyFromConfig(ipdiscovery_wait_seconds, "ipdiscovery_wait_seconds", boost::log::trivial::trace, pt);
  CopyFromConfig(ipuptane_port, "network.ipuptane_port", boost::log::trivial::trace, pt);
}

void NetworkConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, socket_commands_path, "socket_commands_path");
  writeOption(out_stream, socket_events_path, "socket_events_path");
  std::string events_str = "";
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
  CopyFromConfig(server, "server", boost::log::trivial::trace, pt);
  CopyFromConfig(server_url_path, "server_url_path", boost::log::trivial::trace, pt);

  std::string tls_source = "file";
  CopyFromConfig(tls_source, "ca_source", boost::log::trivial::trace, pt);
  if (tls_source == "pkcs11")
    ca_source = kPkcs11;
  else
    ca_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "cert_source", boost::log::trivial::trace, pt);
  if (tls_source == "pkcs11")
    cert_source = kPkcs11;
  else
    cert_source = kFile;

  tls_source = "file";
  CopyFromConfig(tls_source, "pkey_source", boost::log::trivial::trace, pt);
  if (tls_source == "pkcs11")
    pkey_source = kPkcs11;
  else
    pkey_source = kFile;
}

void TlsConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, server, "server");
  writeOption(out_stream, server_url_path, "server_url_path");
  writeOption(out_stream, ca_source, "ca_source");
  writeOption(out_stream, pkey_source, "pkey_source");
  writeOption(out_stream, cert_source, "cert_source");
}

void ProvisionConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(server, "server", boost::log::trivial::trace, pt);
  CopyFromConfig(p12_password, "p12_password", boost::log::trivial::trace, pt);
  CopyFromConfig(expiry_days, "expiry_days", boost::log::trivial::trace, pt);
  CopyFromConfig(provision_path, "provision_path", boost::log::trivial::trace, pt);
  // provision.mode is set in postUpdateValues.
}

void ProvisionConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, server, "server");
  writeOption(out_stream, p12_password, "p12_password");
  writeOption(out_stream, expiry_days, "expiry_days");
  writeOption(out_stream, provision_path, "provision_path");
}

void UptaneConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(polling, "polling", boost::log::trivial::trace, pt);
  CopyFromConfig(polling_sec, "polling_sec", boost::log::trivial::trace, pt);
  CopyFromConfig(device_id, "device_id", boost::log::trivial::trace, pt);
  CopyFromConfig(primary_ecu_serial, "primary_ecu_serial", boost::log::trivial::trace, pt);
  CopyFromConfig(primary_ecu_hardware_id, "primary_ecu_hardware_id", boost::log::trivial::trace, pt);
  CopyFromConfig(director_server, "director_server", boost::log::trivial::trace, pt);
  CopyFromConfig(repo_server, "repo_server", boost::log::trivial::trace, pt);

  std::string ks = "file";
  CopyFromConfig(ks, "key_source", boost::log::trivial::trace, pt);
  if (ks == "pkcs11")
    key_source = kPkcs11;
  else
    key_source = kFile;

  std::string kt;
  CopyFromConfig(kt, "key_type", boost::log::trivial::trace, pt);
  if (kt.size()) {
    if (kt == "RSA2048") {
      key_type = kRSA2048;
    } else if (kt == "RSA4096") {
      key_type = kRSA4096;
    } else if (kt == "ED25519") {
      key_type = kED25519;
    }
  }

  // uptane.secondary_configs currently can only be set via command line.
}

void UptaneConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, polling, "polling");
  writeOption(out_stream, polling_sec, "polling_sec");
  writeOption(out_stream, device_id, "device_id");
  writeOption(out_stream, primary_ecu_serial, "primary_ecu_serial");
  writeOption(out_stream, primary_ecu_hardware_id, "primary_ecu_hardware_id");
  writeOption(out_stream, director_server, "director_server");
  writeOption(out_stream, repo_server, "repo_server");
  writeOption(out_stream, key_source, "key_source");
  writeOption(out_stream, key_type, "key_type");
}

void DiscoveryConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(ipuptane, "ipuptane", boost::log::trivial::trace, pt);
}

void DiscoveryConfig::writeToStream(std::ostream& out_stream) const { writeOption(out_stream, ipuptane, "ipuptane"); }

/**
 * \par Description:
 *    Overload the << operator for the configuration class allowing
 *    us to print its content, i.e. for logging purposes.
 */
std::ostream& operator<<(std::ostream& os, const Config& cfg) {
  return os << "\tPolling enabled : " << cfg.uptane.polling << std::endl
            << "\tPolling interval : " << cfg.uptane.polling_sec << std::endl;
}

Config::Config() { postUpdateValues(); }

Config::Config(const boost::filesystem::path& filename) {
  updateFromDirs();
  updateFromToml(filename);
  postUpdateValues();
}

Config::Config(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd) {
  updateFromDirs();
  updateFromToml(filename);
  updateFromCommandLine(cmd);
  postUpdateValues();
}

KeyManagerConfig Config::keymanagerConfig() const {
  return KeyManagerConfig{p11, tls.ca_source, tls.pkey_source, tls.cert_source, uptane.key_type, uptane.key_source};
}

void Config::postUpdateValues() {
  if (provision.provision_path.empty()) {
    provision.mode = kImplicit;
  }

  if (tls.server.empty()) {
    if (!tls.server_url_path.empty()) {
      tls.server = Utils::readFile(tls.server_url_path);
    } else if (!provision.provision_path.empty()) {
      if (boost::filesystem::exists(provision.provision_path)) {
        tls.server = Bootstrap::readServerUrl(provision.provision_path);
      } else {
        LOG_ERROR << "Provided provision archive '" << provision.provision_path << "' does not exist!";
      }
    }
  }

  if (!tls.server.empty()) {
    if (provision.server.empty()) provision.server = tls.server;

    if (uptane.repo_server.empty()) uptane.repo_server = tls.server + "/repo";

    if (uptane.director_server.empty()) uptane.director_server = tls.server + "/director";

    if (pacman.ostree_server.empty()) pacman.ostree_server = tls.server + "/treehub";
  }
}

void Config::updateFromDirs() {
  for (auto dir : config_dirs_) {
    for (auto config : Utils::glob((dir / "*.conf").string())) {
      updateFromToml(config);
    }
  }
}

void Config::updateFromToml(const boost::filesystem::path& filename) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(filename.string(), pt);
  updateFromPropertyTree(pt);
  LOG_TRACE << "config read from " << filename << " :\n" << (*this);
}

// For testing
void Config::updateFromTomlString(const std::string& contents) {
  boost::property_tree::ptree pt;
  std::stringstream stream(contents);
  boost::property_tree::ini_parser::read_ini(stream, pt);
  updateFromPropertyTree(pt);
}

void Config::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in config.h and writeToFile().
  CopySubtreeFromConfig(gateway, "gateway", pt);
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
}

void Config::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("poll-once") != 0) {
    uptane.polling = false;
  }
  if (cmd.count("gateway-socket") != 0) {
    gateway.socket = cmd["gateway-socket"].as<bool>();
  }
  if (cmd.count("primary-ecu-serial") != 0) {
    uptane.primary_ecu_serial = cmd["primary-ecu-serial"].as<std::string>();
  }
  if (cmd.count("primary-ecu-hardware-id") != 0) {
    uptane.primary_ecu_hardware_id = cmd["primary-ecu-hardware-id"].as<std::string>();
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

  if (cmd.count("secondary-config") != 0) {
    std::vector<boost::filesystem::path> sconfigs = cmd["secondary-config"].as<std::vector<boost::filesystem::path> >();
    readSecondaryConfigs(sconfigs);
  }

  if (cmd.count("legacy-interface") != 0) {
    boost::filesystem::path legacy_interface = cmd["legacy-interface"].as<boost::filesystem::path>();
    checkLegacyVersion(legacy_interface);
    initLegacySecondaries(legacy_interface);
  }
}

void Config::readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs) {
  std::vector<boost::filesystem::path>::const_iterator it;
  for (it = sconfigs.begin(); it != sconfigs.end(); ++it) {
    if (!boost::filesystem::exists(*it)) {
      throw FatalException(it->string() + " does not exist!");
    }
    Json::Value config_json = Utils::parseJSONFile(*it);
    Uptane::SecondaryConfig sconfig;

    std::string stype = config_json["secondary_type"].asString();
    if (stype == "virtual") {
      sconfig.secondary_type = Uptane::kVirtual;
    } else if (stype == "legacy") {
      LOG_ERROR << "Legacy secondaries should be initialized with --legacy-interface.";
      continue;
    } else if (stype == "ip_uptane") {
      sconfig.secondary_type = Uptane::kIpUptane;
    } else if (stype == "opcua_uptane") {
      sconfig.secondary_type = Uptane::kOpcuaUptane;
    } else {
      LOG_ERROR << "Unrecognized secondary type: " << stype;
      continue;
    }
    sconfig.ecu_serial = config_json["ecu_serial"].asString();
    sconfig.ecu_hardware_id = config_json["ecu_hardware_id"].asString();
    sconfig.partial_verifying = config_json["partial_verifying"].asBool();
    sconfig.ecu_private_key = config_json["ecu_private_key"].asString();
    sconfig.ecu_public_key = config_json["ecu_public_key"].asString();

    sconfig.full_client_dir = boost::filesystem::path(config_json["full_client_dir"].asString());
    sconfig.firmware_path = boost::filesystem::path(config_json["firmware_path"].asString());
    sconfig.metadata_path = boost::filesystem::path(config_json["metadata_path"].asString());
    sconfig.target_name_path = boost::filesystem::path(config_json["target_name_path"].asString());
    sconfig.flasher = "";

    std::string key_type = config_json["key_type"].asString();
    if (key_type.size()) {
      if (key_type == "RSA2048") {
        sconfig.key_type = kRSA2048;
      } else if (key_type == "RSA4096") {
        sconfig.key_type = kRSA4096;
      } else if (key_type == "ED25519") {
        sconfig.key_type = kED25519;
      }
    }
    uptane.secondary_configs.push_back(sconfig);
  }
}

void Config::checkLegacyVersion(const boost::filesystem::path& legacy_interface) {
  if (!boost::filesystem::exists(legacy_interface)) {
    throw FatalException(std::string("Legacy external flasher not found: ") + legacy_interface.string());
  }
  std::stringstream command;
  std::string output;
  command << legacy_interface << " api-version --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    throw FatalException(std::string("Legacy external flasher api-version command failed: ") + output);
  } else {
    boost::trim_if(output, boost::is_any_of(" \n\r\t"));
    if (output != "1") {
      throw FatalException(std::string("Unexpected legacy external flasher API version: ") + output);
    }
  }
}

void Config::initLegacySecondaries(const boost::filesystem::path& legacy_interface) {
  std::stringstream command;
  std::string output;
  command << legacy_interface << " list-ecus --loglevel " << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);
  if (rs != 0) {
    LOG_ERROR << "Legacy external flasher list-ecus command failed: " << output;
    return;
  }

  std::stringstream ss(output);
  std::string buffer;
  while (std::getline(ss, buffer, '\n')) {
    Uptane::SecondaryConfig sconfig;
    sconfig.secondary_type = Uptane::kLegacy;
    std::vector<std::string> ecu_info;
    boost::split(ecu_info, buffer, boost::is_any_of(" \n\r\t"), boost::token_compress_on);
    if (ecu_info.size() == 0) {
      // Could print a warning but why bother.
      continue;
    } else if (ecu_info.size() == 1) {
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
    sconfig.flasher = legacy_interface;

    uptane.secondary_configs.push_back(sconfig);
  }
}

// This writes out every configuration option, including those set with default
// and blank values. This may be useful for replicating an exact configuration
// environment. However, if we were to want to simplify the output file, we
// could skip blank strings or compare values against a freshly built instance
// to detect and skip default values.
void Config::writeToFile(const boost::filesystem::path& filename) const {
  // Keep this order the same as in config.h and updateFromPropertyTree().
  std::ofstream sink(filename.c_str(), std::ofstream::out);

  WriteSectionToStream(gateway, "gateway", sink);
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
}

asn1::Serializer& operator<<(asn1::Serializer& ser, CryptoSource cs) {
  ser << asn1::implicit<kAsn1Enum>(static_cast<const int32_t&>(cs));

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

  if (cs_i < kFile || cs_i > kPkcs11) throw deserialization_error();

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
