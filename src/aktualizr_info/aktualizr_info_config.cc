#include "aktualizr_info_config.h"

#include "utilities/config_utils.h"

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
    for (const auto& config : configs) {
      if (!boost::filesystem::exists(config)) {
        LOG_ERROR << "Provided config file or directory " << config << " does not exist!";
      }
    }
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

void AktualizrInfoConfig::postUpdateValues() { logger_set_threshold(logger); }

void AktualizrInfoConfig::updateFromCommandLine(const boost::program_options::variables_map& cmd) {
  if (cmd.count("loglevel") != 0) {
    logger.loglevel = cmd["loglevel"].as<int>();
  }
}

void AktualizrInfoConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  // Keep this order the same as in aktualizr_info_config.h and
  // AktualizrInfoConfig::writeToFile().

  // from aktualizr config
  if (!loglevel_from_cmdline) {
    CopySubtreeFromConfig(logger, "logger", pt);
    // If not already set from the commandline, set the loglevel now so that it
    // affects the rest of the config processing.
    logger_set_threshold(logger);
  }

  // from aktualizr config
  CopySubtreeFromConfig(storage, "storage", pt);
}

std::ostream& operator<<(std::ostream& os, const AktualizrInfoConfig& cfg) {
  (void)cfg;
  return os;
}

void AktualizrInfoConfig::writeToFile(const boost::filesystem::path& filename) {
  // Keep this order the same as in aktualizr_info_config.h and
  // AktualizrInfoConfig::updateFromPropertyTree().
  std::ofstream sink(filename.c_str(), std::ofstream::out);
  sink << std::boolalpha;

  // from aktualizr config
  WriteSectionToStream(logger, "logger", sink);
  WriteSectionToStream(storage, "storage", sink);
}
