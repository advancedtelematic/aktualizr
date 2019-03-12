#ifndef UPTANE_SECONDARYFACTORY_H_
#define UPTANE_SECONDARYFACTORY_H_

#include <memory>
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"

namespace Uptane {

class SecondaryFactory {
 public:
  static std::shared_ptr<SecondaryInterface> makeSecondary(const SecondaryConfig& sconfig);
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYFACTORY_H_
