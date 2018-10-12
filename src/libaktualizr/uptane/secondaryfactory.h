#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include "ipuptanesecondary.h"
#include "isotpsecondary.h"
#include "logging/logging.h"
#include "uptane/ipuptanesecondary.h"
#include "uptane/opcuasecondary.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/virtualsecondary.h"

namespace Uptane {

class SecondaryFactory {
 public:
  static std::shared_ptr<SecondaryInterface> makeSecondary(const SecondaryConfig& sconfig) {
    switch (sconfig.secondary_type) {
      case SecondaryType::kVirtual:
        return std::make_shared<VirtualSecondary>(sconfig);
      case SecondaryType::kLegacy:
        LOG_ERROR << "Legacy secondary support is deprecated.";
        return std::shared_ptr<SecondaryInterface>();  // NULL-equivalent
      case SecondaryType::kIpUptane:
        return std::make_shared<IpUptaneSecondary>(sconfig);
      case SecondaryType::kIsoTpUptane:
        return std::make_shared<IsoTpSecondary>(sconfig);
      case SecondaryType::kOpcuaUptane:
#ifdef OPCUA_SECONDARY_ENABLED
        return std::make_shared<OpcuaSecondary>(sconfig);
#else
        LOG_ERROR << "libaktualizr was built without OPC-UA secondary support.";
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
