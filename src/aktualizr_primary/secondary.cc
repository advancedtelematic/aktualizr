#include <unordered_map>

#include "secondary.h"
#include "secondary_config.h"

#include "ipuptanesecondary.h"

namespace Primary {

using Secondaries = std::vector<std::shared_ptr<Uptane::SecondaryInterface>>;
using SecondaryFactoryRegistry = std::unordered_map<std::string, std::function<Secondaries(const SecondaryConfig&)>>;

static Secondaries createIPSecondaries(const IPSecondariesConfig& config);

static SecondaryFactoryRegistry sec_factory_registry = {
    {IPSecondariesConfig::Type,
     [](const SecondaryConfig& config) {
       auto ip_sec_cgf = dynamic_cast<const IPSecondariesConfig&>(config);
       return createIPSecondaries(ip_sec_cgf);
     }},
    //  {
    //     Add another secondary factory here
    //  }
};

static Secondaries createSecondaries(const SecondaryConfig& config) {
  return (sec_factory_registry.at(config.type()))(config);
}

void initSecondaries(Aktualizr& aktualizr, const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Secondary ECUs config file does not exist: " + config_file.string());
  }

  auto secondary_configs = SecondaryConfigParser::parse_config_file(config_file);

  for (auto& config : secondary_configs) {
    try {
      LOG_INFO << "Creating " << config->type() << " secondaries...";
      Secondaries secondaries = createSecondaries(*config);

      for (auto secondary : secondaries) {
        LOG_INFO << "Adding Secondary to Aktualizr."
                 << "HW_ID: " << secondary->getHwId() << " Serial: " << secondary->getSerial();
        aktualizr.AddSecondary(secondary);
      }
    } catch (const std::exception& exc) {
      LOG_ERROR << "Failed to initialize a secondary: " << exc.what();
      LOG_ERROR << "Continue with initialization of the remaining secondaries, if any left.";
      // otherwise rethrow the exception
    }
  }
}

static Secondaries createIPSecondaries(const IPSecondariesConfig& config) {
  Secondaries result;

  for (auto& ip_sec_cfg : config.secondaries_cfg) {
    result.push_back(Uptane::IpUptaneSecondary::create(ip_sec_cfg.ip, ip_sec_cfg.port));
  }
  return result;
}

}  // namespace Primary
