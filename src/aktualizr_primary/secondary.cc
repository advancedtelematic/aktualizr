#include <unordered_map>

#include "secondary.h"
#include "secondary_config.h"

#include "ipuptanesecondary.h"

namespace Primary {

using SecondaryFactoryRegistry =
    std::unordered_map<std::string, std::function<std::shared_ptr<Uptane::SecondaryInterface>(const SecondaryConfig&)>>;

static SecondaryFactoryRegistry sec_factory_registry = {
    {IPSecondaryConfig::Type,
     [](const SecondaryConfig& config) {
       auto ip_sec_cgf = dynamic_cast<const IPSecondaryConfig&>(config);
       return Uptane::IpUptaneSecondary::create(ip_sec_cgf.ip, ip_sec_cgf.port);
     }},
    //  {
    //     Add another secondary factory here
    //  }
};

static std::shared_ptr<Uptane::SecondaryInterface> createSecondary(const SecondaryConfig& config) {
  return (sec_factory_registry.at(config.type()))(config);
}

void initSecondaries(Aktualizr& aktualizr, const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Secondary ECUs config file does not exist: " + config_file.string());
  }

  auto secondary_configs = SecondaryConfigParser::parse_config_file(config_file);

  for (auto& config : secondary_configs) {
    try {
      LOG_INFO << "Creating Secondary of type: " << config->type();
      std::shared_ptr<Uptane::SecondaryInterface> secondary = createSecondary(*config);

      LOG_INFO << "Adding Secondary to Aktualizr."
               << "HW_ID: " << secondary->getHwId() << " Serial: " << secondary->getSerial();
      aktualizr.AddSecondary(secondary);

    } catch (const std::exception& exc) {
      LOG_ERROR << "Failed to initialize a secondary: " << exc.what();
      LOG_ERROR << "Continue with initialization of the remaining secondaries, if any left.";
      // otherwise rethrow the exception
    }
  }
}
}  // namespace Primary
