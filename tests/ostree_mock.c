#define _GNU_SOURCE
#include <dlfcn.h>
#include <ostree.h>
#include <stdio.h>

OstreeDeployment *ostree_sysroot_get_booted_deployment (OstreeSysroot *self) {
  OstreeDeployment* (*orig)(OstreeSysroot*) = dlsym(RTLD_NEXT, __func__);
  char mocked_name[100];
  snprintf(mocked_name, sizeof(mocked_name), "%s_mock", __func__);
  OstreeDeployment* (*mocked)(OstreeSysroot*) = dlsym(RTLD_DEFAULT, mocked_name);

  if (mocked) {
    return mocked(self);
  }
  return orig(self);
}

const char *ostree_deployment_get_bootcsum (OstreeDeployment *self) {
  char* (*orig)(OstreeDeployment*) = dlsym(RTLD_NEXT, __func__);
  char mocked_name[100];
  snprintf(mocked_name, sizeof(mocked_name), "%s_mock", __func__);
  char* (*mocked)(OstreeDeployment*) = dlsym(RTLD_DEFAULT, mocked_name);

  if (mocked) {
    return mocked(self);
  }
  return orig(self);
}
