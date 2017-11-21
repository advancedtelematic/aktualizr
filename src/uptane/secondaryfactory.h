#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "logger.h"
#include "uptane/econdaryinterface.h"
#include "uptane/secondaryconfig.h"

namespace Uptane {

class SecondaryFactory {
 public:
  boost::shared_ptr<SecondaryInterface> makeSecondary(const SecondaryConfig& sconfig) {
    switch (sconfig.SecondaryType) {
      case kVirtual:
        return boost::make_shared<VirtualSecondary>(sconfig);
        break;
      case kLegacy:
        return boost::make_shared<LegacySecondary>(sconfig);
        break;
      case kUptane:
        LOGGER_LOG(LVL_error, "Uptane secondaries are not currently supported.");
        return NULL;
      default:
        LOGGER_LOG(LVL_error, "Unrecognized secondary type: " << sconfig.SecondaryType);
        return NULL;
    }
  }
};
}

#endif  // UPTANE_SECONDARYFACTORY_H_
