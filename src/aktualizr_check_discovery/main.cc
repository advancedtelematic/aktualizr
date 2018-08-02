#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "config/config.h"
#include "uptane/secondaryfactory.h"
#include "uptane/secondaryinterface.h"

#include "logging/logging.h"
#include "uptane/ipsecondarydiscovery.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("aktualizr-check-discovery command line options");
  // clang-format off
  desc.add_options()
    ("help,h", "print usage")
    ("config,c", po::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory or file");
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

    std::vector<boost::filesystem::path> sota_config_files;
    if (vm.count("config") > 0) {
      sota_config_files = vm["config"].as<std::vector<boost::filesystem::path>>();
    }
    Config config(sota_config_files);

    IpSecondaryDiscovery ip_uptane_discovery{config.network};
    auto discovered = ip_uptane_discovery.discover();
    LOG_INFO << "Discovering finished";
    LOG_INFO << "Found " << discovered.size() << " devices";
    if (discovered.size() != 0u) {
      LOG_TRACE << "Trying to connect with detected devices and get public key";
      bool all_ok = true;

      std::vector<Uptane::SecondaryConfig>::const_iterator it;
      for (it = discovered.begin(); it != discovered.end(); ++it) {
        std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(*it);
        if (it->secondary_type == Uptane::SecondaryType::kIpUptane) {
          auto public_key = sec->getPublicKey();
          LOG_INFO << "Got public key from secondary: " << public_key.ToUptane();
          auto manifest = sec->getManifest();
          LOG_INFO << "Got manifest: " << manifest;
          if (manifest.isMember("signatures") && manifest.isMember("signed")) {
            std::string canonical = Json::FastWriter().write(manifest["signed"]);
            bool verified = public_key.VerifySignature(manifest["signatures"][0]["sig"].asString(), canonical);
            if (verified) {
              LOG_INFO << "Manifest has been successfully verified";
            } else {
              LOG_INFO << "Manifest verification failed";
              all_ok = false;
            }
          } else {
            LOG_INFO << "Manifest is corrupted or not signed";
            all_ok = false;
          }
        }
      }  // foreach secondary

      if (all_ok) {
        LOG_INFO << "All secondaries provided valid manifests";
        return EXIT_SUCCESS;
      } else {
        LOG_WARNING << "One or more secondaries did not return a correct manifest";
        return EXIT_FAILURE;
      }
    } else {
      LOG_INFO << "No secondaries were discovered";
      return EXIT_FAILURE;
    }
  } catch (const po::error &o) {
    std::cout << o.what() << std::endl;
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
