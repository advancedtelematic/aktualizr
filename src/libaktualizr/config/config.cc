#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

#include "bootstrap/bootstrap.h"
#include "config.h"
#include "utilities/exceptions.h"
#include "utilities/utils.h"

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
  CopyFromConfig(polling_sec, "polling_sec", pt);
  CopyFromConfig(director_server, "director_server", pt);
  CopyFromConfig(repo_server, "repo_server", pt);
  CopyFromConfig(key_source, "key_source", pt);
  CopyFromConfig(key_type, "key_type", pt);
  CopyFromConfig(force_install_completion, "force_install_completion", pt);
  CopyFromConfig(secondary_config_file, "secondary_config_file", pt);
  CopyFromConfig(secondary_preinstall_wait_sec, "secondary_preinstall_wait_sec", pt);
}

void UptaneConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, polling_sec, "polling_sec");
  writeOption(out_stream, director_server, "director_server");
  writeOption(out_stream, repo_server, "repo_server");
  writeOption(out_stream, key_source, "key_source");
  writeOption(out_stream, key_type, "key_type");
  writeOption(out_stream, force_install_completion, "force_install_completion");
  writeOption(out_stream, secondary_config_file, "secondary_config_file");
  writeOption(out_stream, secondary_preinstall_wait_sec, "secondary_preinstall_wait_sec");
}

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

  provision.mode = provision.provision_path.empty() ? ProvisionMode::kDeviceCred : ProvisionMode::kSharedCred;

  if (tls.server.empty()) {
    if (!tls.server_url_path.empty()) {
      try {
        tls.server = Utils::readFile(tls.server_url_path, true);
      } catch (const std::exception& e) {
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
  CopySubtreeFromConfig(p11, "p11", pt);
  CopySubtreeFromConfig(tls, "tls", pt);
  CopySubtreeFromConfig(provision, "provision", pt);
  CopySubtreeFromConfig(uptane, "uptane", pt);
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
  if (cmd.count("tls-server") != 0) {
    tls.server = cmd["tls-server"].as<std::string>();
  }
  if (cmd.count("repo-server") != 0) {
    uptane.repo_server = cmd["repo-server"].as<std::string>();
  }
  if (cmd.count("director-server") != 0) {
    uptane.director_server = cmd["director-server"].as<std::string>();
  }
  if (cmd.count("primary-ecu-serial") != 0) {
    provision.primary_ecu_serial = cmd["primary-ecu-serial"].as<std::string>();
  }
  if (cmd.count("primary-ecu-hardware-id") != 0) {
    provision.primary_ecu_hardware_id = cmd["primary-ecu-hardware-id"].as<std::string>();
  }
  if (cmd.count("secondary-config-file") != 0) {
    uptane.secondary_config_file = cmd["secondary_config_file"].as<boost::filesystem::path>();
  }
}

void Config::writeToStream(std::ostream& sink) const {
  // Keep this order the same as in config.h and
  // Config::updateFromPropertyTree().
  WriteSectionToStream(logger, "logger", sink);
  WriteSectionToStream(p11, "p11", sink);
  WriteSectionToStream(tls, "tls", sink);
  WriteSectionToStream(provision, "provision", sink);
  WriteSectionToStream(uptane, "uptane", sink);
  WriteSectionToStream(pacman, "pacman", sink);
  WriteSectionToStream(storage, "storage", sink);
  WriteSectionToStream(import, "import", sink);
  WriteSectionToStream(telemetry, "telemetry", sink);
  WriteSectionToStream(bootloader, "bootloader", sink);
}
