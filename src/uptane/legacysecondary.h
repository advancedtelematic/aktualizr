#ifndef UPTANE_LEGACYSECONDARY_H_
#define UPTANE_LEGACYSECONDARY_H_

#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

#include <vector>

/* Json snippet returned by sendMetaXXX():
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */
namespace Uptane {
class LegacySecondary : public SecondaryInterface {
 public:
  LegacySecondary();

 private:
};
}

#endif  // UPTANE_LEGACYSECONDARY_H_
