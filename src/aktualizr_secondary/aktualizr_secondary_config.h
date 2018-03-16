#ifndef AKTUALIZR_SECONDARY_CONFIG_H_
#define AKTUALIZR_SECONDARY_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "config.h"
#include "invstorage.h"

struct AktualizrSecondaryNetConfig {
  int port{9030};
  bool discovery{true};
  int discovery_port{9031};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class AktualizrSecondaryConfig {
 public:
  AktualizrSecondaryConfig() {}
  AktualizrSecondaryConfig(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd);
  AktualizrSecondaryConfig(const boost::filesystem::path& filename);

  void writeToFile(const boost::filesystem::path& filename);

  AktualizrSecondaryNetConfig network;

  // from primary config
  StorageConfig storage;
  P11Config p11;
  UptaneConfig uptane;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);
};

#endif  // AKTUALIZR_SECONDARY_CONFIG_H_
