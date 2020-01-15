#ifndef AKTUALIZR_SECONDARY_FACTORY_H
#define AKTUALIZR_SECONDARY_FACTORY_H

#include "aktualizr_secondary.h"

class AktualizrSecondaryFactory {
 public:
  static AktualizrSecondary::Ptr create(const AktualizrSecondaryConfig& config);
  static AktualizrSecondary::Ptr create(const AktualizrSecondaryConfig& config,
                                        const std::shared_ptr<INvStorage>& storage);
};

#endif  // AKTUALIZR_SECONDARY_FACTORY_H
