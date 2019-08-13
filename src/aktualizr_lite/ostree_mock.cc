#include <ostree.h>

extern "C" OstreeDeployment *ostree_sysroot_get_booted_deployment(OstreeSysroot *self) {
  (void)self;
  const char *hash = getenv("OSTREE_HASH");
  return ostree_deployment_new(0, "dummy-os", hash, 1, hash, 1);
}

extern "C" const char *ostree_deployment_get_csum(OstreeDeployment *self) {
  (void)self;
  return getenv("OSTREE_HASH");
}
