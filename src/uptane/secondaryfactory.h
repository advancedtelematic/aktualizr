#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "logger.h"
#include "uptane/legacysecondary.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/virtualsecondary.h"

namespace Uptane {

class SecondaryFactory {
 public:
  static boost::shared_ptr<SecondaryInterface> makeSecondary(const SecondaryConfig& sconfig) {
    switch (sconfig.secondary_type) {
      case kVirtual:
        return boost::make_shared<VirtualSecondary>(sconfig);
        break;
      case kLegacy:
        return boost::make_shared<LegacySecondary>(sconfig);
        break;
      case kUptane:
        LOGGER_LOG(LVL_error, "Uptane secondaries are not currently supported.");
        return boost::shared_ptr<SecondaryInterface>();  // NULL-equivalent
      default:
        LOGGER_LOG(LVL_error, "Unrecognized secondary type: " << sconfig.secondary_type);
        return boost::shared_ptr<SecondaryInterface>();  // NULL-equivalent
    }
  }
};
}

#endif  // UPTANE_SECONDARYFACTORY_H_
