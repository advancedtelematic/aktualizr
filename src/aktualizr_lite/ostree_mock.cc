#include <ostree.h>
#include <stdlib.h>

extern "C" OstreeDeployment *ostree_sysroot_get_booted_deployment(OstreeSysroot *self) {
  (void)self;
  static OstreeDeployment *deployment;

  if (deployment != nullptr) {
    return deployment;
  }

  const char *hash = getenv("OSTREE_HASH");
  deployment = ostree_deployment_new(0, "dummy-os", hash, 1, hash, 1);
  return deployment;
}

extern "C" const char *ostree_deployment_get_csum(OstreeDeployment *self) {
  (void)self;
  return getenv("OSTREE_HASH");
}
