#ifndef AKTUALIZR_SECONDARY_CONFIG_H_
#define AKTUALIZR_SECONDARY_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

struct AktualizrSecondaryNetConfig {
  int port{9030};
};

class AktualizrSecondaryConfig {
 public:
  AktualizrSecondaryConfig() {}
  AktualizrSecondaryConfig(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd);
  AktualizrSecondaryConfig(const boost::filesystem::path& filename);

  void writeToFile(const boost::filesystem::path& filename);

  AktualizrSecondaryNetConfig network;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);
};

#endif  // AKTUALIZR_SECONDARY_CONFIG_H_
