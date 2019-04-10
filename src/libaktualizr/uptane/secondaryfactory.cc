#include "uptane/secondaryfactory.h"

#include "logging/logging.h"
#include "uptane/virtualsecondary.h"

#ifdef ISOTP_SECONDARY_ENABLED
#include "isotpsecondary.h"
#endif

#ifdef OPCUA_SECONDARY_ENABLED
#include "uptane/opcuasecondary.h"
#endif

namespace Uptane {

std::shared_ptr<SecondaryInterface> SecondaryFactory::makeSecondary(const SecondaryConfig& sconfig) {
  switch (sconfig.secondary_type) {
    case SecondaryType::kVirtual:
      return std::make_shared<VirtualSecondary>(sconfig);
    case SecondaryType::kLegacy:
      LOG_ERROR << "Legacy secondary support is deprecated.";
      return std::shared_ptr<SecondaryInterface>();  // NULL-equivalent
    case SecondaryType::kIsoTpUptane:
#ifdef ISOTP_SECONDARY_ENABLED
      return std::make_shared<IsoTpSecondary>(sconfig);
#else
      LOG_ERROR << "libaktualizr was built without ISO/TP secondary support.";
      return std::shared_ptr<SecondaryInterface>();  // NULL-equivalent
#endif
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

}  // namespace Uptane
