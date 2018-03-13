#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "logging.h"
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
        LOG_ERROR << "Uptane secondaries are not currently supported.";
        return boost::shared_ptr<SecondaryInterface>();  // NULL-equivalent
      default:
        LOG_ERROR << "Unrecognized secondary type: " << sconfig.secondary_type;
        return boost::shared_ptr<SecondaryInterface>();  // NULL-equivalent
    }
  }
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYFACTORY_H_
