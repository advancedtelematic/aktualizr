#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include "logging/logging.h"
#include "uptane/ipuptanesecondary.h"
#include "uptane/legacysecondary.h"
#include "uptane/opcuasecondary.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/virtualsecondary.h"
#include "utilities/events.h"

namespace Uptane {

class SecondaryFactory {
 public:
  static std::shared_ptr<SecondaryInterface> makeSecondary(const SecondaryConfig& sconfig) {
    switch (sconfig.secondary_type) {
      case SecondaryType::kVirtual:
        return std::make_shared<VirtualSecondary>(sconfig);
        break;
      case SecondaryType::kLegacy:
        return std::make_shared<LegacySecondary>(sconfig);
        break;
      case SecondaryType::kIpUptane:
        return std::make_shared<IpUptaneSecondary>(sconfig);
      case SecondaryType::kOpcuaUptane:
#ifdef OPCUA_SECONDARY_ENABLED
        return std::make_shared<OpcuaSecondary>(sconfig);
#else
        LOG_ERROR << "Built with no OPC-UA secondary support";
        return std::shared_ptr<SecondaryInterface>();  // NULL-equivalent
#endif
      default:
        LOG_ERROR << "Unrecognized secondary type: " << static_cast<int>(sconfig.secondary_type);
        return std::shared_ptr<SecondaryInterface>();  // NULL-equivalent
    }
  }
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYFACTORY_H_
