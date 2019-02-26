#ifndef DOCKERAPPMGR_H_
#define DOCKERAPPMGR_H_

#include "ostreemanager.h"

class DockerAppManager : public OstreeManager {
 public:
  DockerAppManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage, std::shared_ptr<Bootloader> bootloader,
                   std::shared_ptr<HttpInterface> http)
      : OstreeManager(pconfig, storage, bootloader, http) {}
  std::string name() const override { return "ostree+docker-app"; }
};

#endif  // DOCKERAPPMGR_H_
