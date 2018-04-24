#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "config.h"
#include "uptane/secondaryfactory.h"
#include "uptane/secondaryinterface.h"

#include "ipsecondarydiscovery.h"
#include "logging/logging.h"
#include "secondary_ipc/ipuptaneconnection.h"
#include "secondary_ipc/ipuptaneconnectionsplitter.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("aktualizr_info command line options");
  // clang-format off
  desc.add_options()
    ("help,h", "print usage")
    ("config,c", po::value<std::string>(), "toml configuration file");
  // clang-format on

  try {
    logger_init();
    logger_set_threshold(boost::log::trivial::info);
    po::variables_map vm;
    po::basic_parsed_options<char> parsed_options = po::command_line_parser(argc, argv).options(desc).run();
    po::store(parsed_options, vm);
    po::notify(vm);
    if (vm.count("help") != 0) {
      std::cout << desc << '\n';
      exit(EXIT_SUCCESS);
    }

    std::string sota_config_file = "/usr/lib/sota/sota.toml";
    if (vm.count("config") != 0) {
      sota_config_file = vm["config"].as<std::string>();
    } else if (boost::filesystem::exists("/tmp/aktualizr_config_path")) {
      sota_config_file = Utils::readFile("/tmp/aktualizr_config_path");
    }

    boost::filesystem::path sota_config_path(sota_config_file);
    if (!boost::filesystem::exists(sota_config_path)) {
      std::cout << "configuration file " << boost::filesystem::absolute(sota_config_path) << " not found. Exiting."
                << std::endl;
      exit(EXIT_FAILURE);
    }
    Config config(sota_config_path.string());

    IpSecondaryDiscovery ip_uptane_discovery{config.network};
    auto discovered = ip_uptane_discovery.discover();
    LOG_INFO << "Discovering finished";
    LOG_INFO << "Found " << discovered.size() << " devices";
    if (discovered.size() != 0u) {
      LOG_TRACE << "Trying to connect with founded devices and get public key";
      IpUptaneConnection ip_uptane_connection(config.network.ipuptane_port);
      IpUptaneConnectionSplitter ip_uptane_splitter(ip_uptane_connection);

      std::vector<Uptane::SecondaryConfig>::const_iterator it;
      for (it = discovered.begin(); it != discovered.end(); ++it) {
        std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(*it);
        if (it->secondary_type == Uptane::kIpUptane) {
          ip_uptane_splitter.registerSecondary(*dynamic_cast<Uptane::IpUptaneSecondary *>(&(*sec)));
          auto public_key = sec->getPublicKey();
          LOG_INFO << "Got public key from secondary: " << public_key.second;
          auto manifest = sec->getManifest();
          LOG_INFO << "Got manifest: " << manifest;
          if (manifest.isMember("signatures") && manifest.isMember("signed")) {
            PublicKey public_key_object(public_key.second, keyTypeToString(public_key.first));
            std::string canonical = Json::FastWriter().write(manifest["signed"]);
            bool verified =
                Crypto::VerifySignature(public_key_object, manifest["signatures"][0]["sig"].asString(), canonical);
            if (verified) {
              LOG_INFO << "Manifest has been successfully verified";
            } else {
              LOG_INFO << "Manifest verification failed";
            }
          } else {
            LOG_INFO << "Manifest is corrupted or not signed";
          }
        }
      }
      exit(EXIT_SUCCESS);
    }
    LOG_INFO << "Exiting";

  } catch (const po::error &o) {
    std::cout << o.what() << std::endl;
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
