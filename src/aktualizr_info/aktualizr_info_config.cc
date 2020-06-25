#include "aktualizr_info_config.h"
#include "utilities/config_utils.h"
#include "logging/logging.h"


AktualizrInfoConfig::AktualizrInfoConfig(const boost::program_options::variables_map& cmd) {
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

AktualizrInfoConfig::AktualizrInfoConfig(const boost::filesystem::path& filename) {
  updateFromToml(filename);
  postUpdateValues();
}

void AktualizrInfoConfig::postUpdateValues() {
  logger_set_threshold(logger);
  LOG_TRACE << "Final configuration that will be used: \n" << (*this);
}

void AktualizrInfoConfig::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
  }
}

void AktualizrInfoConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in aktualizr_info_config.h and
  // AktualizrInfoConfig::writeToStream().

  // from aktualizr config
  if (!loglevel_from_cmdline) {
    CopySubtreeFromConfig(logger, "logger", pt);
    // If not already set from the commandline, set the loglevel now so that it
    // affects the rest of the config processing.
    logger_set_threshold(logger);
  }

  // from aktualizr config
  CopySubtreeFromConfig(pacman, "pacman", pt);
  CopySubtreeFromConfig(storage, "storage", pt);
}

void AktualizrInfoConfig::writeToStream(std::ostream& sink) const {
  // Keep this order the same as in aktualizr_info_config.h and
  // AktualizrInfoConfig::updateFromPropertyTree().
  WriteSectionToStream(logger, "logger", sink);
  WriteSectionToStream(pacman, "pacman", sink);
  WriteSectionToStream(storage, "storage", sink);
}

std::ostream& operator<<(std::ostream& os, const AktualizrInfoConfig& cfg) {
  cfg.writeToStream(os);
  return os;
}
