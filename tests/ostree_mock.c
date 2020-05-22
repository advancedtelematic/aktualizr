#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <ostree.h>
#include <stdio.h>

/**
 * @brief This mock allows the OSTree package manager usage on non-OSTree booted environment, e.g. local host
 *
 * in order to use it the following has to be pre-defined
 * 1) OSTREE_DEPLOYMENT_VERSION_FILE environment variable that points to the test file containing a revision
 * hash/checksum and its serial number 2) LD_PRELOAD environment variable that refers to the libostree_mock.so 3)
 * [pacman].os == OSTREE_DEPLOYMENT_OS_NAME (aktualizr configuration, pacman section, os field must be equal to
 * OSTREE_DEPLOYMENT_OS_NAME defined here)
 *
 * OSTREE_DEPLOYMENT_VERSION_FILE must follow this schema/format
 *
 * <deployed-revision-hash>\n
 * <deployed-revision-serial>\n
 * <booted-revision-hash>\n
 * <booted-revision-serial>
 *
 * For example
 *
 *  d0ef921904bc573616a42e862de2421b265a76d06a334cfe5ad9f811107bbee0
 *  0
 *  d0ef921904bc573616a42e862de2421b265a76d06a334cfe5ad9f811107bbee0
 *  0
 */

static const char OSTREE_DEPLOYMENT_OS_NAME[] = "dummy-os";
static const char OSTREE_DEPLOYMENT_VERSION_FILE[] = "OSTREE_DEPLOYMENT_VERSION_FILE";

struct OstreeDeploymentVersion {
  int serial;
  char rev[256];
};

static int get_ostree_deployment_version(const char* filename, struct OstreeDeploymentVersion* deployed,
                                         struct OstreeDeploymentVersion* booted);

struct OstreeDeploymentInfo {
  struct OstreeDeploymentVersion deployed;
  struct OstreeDeploymentVersion booted;
  OstreeDeployment* booted_ostree_native_deployment;
};

static struct OstreeDeploymentInfo* get_ostree_deployment_info() {
  // NOTE: Is not tread-safe. This mock is used solely for testing and normally is called from one thread of the
  // Aktualirz
  static struct OstreeDeploymentInfo DeploymentInfo;
  static int isDeploymentInfoLoaded = 0;

  if (isDeploymentInfoLoaded) {
    return &DeploymentInfo;
  }

  const char* deployment_version_file = NULL;
  if ((deployment_version_file = getenv(OSTREE_DEPLOYMENT_VERSION_FILE)) == NULL) {
    return NULL;
  }

  if (get_ostree_deployment_version(deployment_version_file, &DeploymentInfo.deployed, &DeploymentInfo.booted) != 0) {
    return NULL;
  }

  // Allocation of the singletone object in a heap, one instance per a process, a memory will be released during
  // the process teardown, no need in explicit deallocation
  DeploymentInfo.booted_ostree_native_deployment =
      ostree_deployment_new(0, OSTREE_DEPLOYMENT_OS_NAME, DeploymentInfo.deployed.rev, DeploymentInfo.deployed.serial,
                            DeploymentInfo.booted.rev, DeploymentInfo.booted.serial);

  assert(DeploymentInfo.booted_ostree_native_deployment != NULL);

  isDeploymentInfoLoaded = 1;

  return &DeploymentInfo;
}

const char* ostree_deployment_get_csum(OstreeDeployment* self) {
  const char* (*orig)(OstreeDeployment*) = (const char* (*)(OstreeDeployment*))(dlsym(RTLD_NEXT, __func__));
  struct OstreeDeploymentInfo* deployment_info = get_ostree_deployment_info();

  if (deployment_info != NULL) {
    return deployment_info->deployed.rev;
  }

  return orig(self);
}

OstreeDeployment* ostree_sysroot_get_booted_deployment(OstreeSysroot* self) {
  OstreeDeployment* (*orig)(OstreeSysroot*) = (OstreeDeployment * (*)(OstreeSysroot*))(dlsym(RTLD_NEXT, __func__));
  struct OstreeDeploymentInfo* deployment_info = get_ostree_deployment_info();

  if (deployment_info != NULL) {
    return deployment_info->booted_ostree_native_deployment;
  }

  return orig(self);
}

static int read_deployment_info(FILE* file, struct OstreeDeploymentVersion* deployment) {
  struct OstreeDeploymentVersion deployment_res;

  // TODO: make it more robust and reliable as a file might contain a value exceeding 255
  if (fscanf(file, "%254s\n", deployment_res.rev) != 1) {
    return -1;
  }

  // TODO: make it more robust and reliable as a file might contain a value exceeding 255 and/or char(s) instead of a
  // number for the serial value
  if (fscanf(file, "%d\n", &deployment_res.serial) != 1) {
    return -1;
  }

  *deployment = deployment_res;
  return 0;
}

static int get_ostree_deployment_version(const char* filename, struct OstreeDeploymentVersion* deployed,
                                         struct OstreeDeploymentVersion* booted) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    printf("\n>>>>>>>>>> Failed to open the file: %s\n\n", filename);
    return -1;
  }

  int return_code = -1;

  do {
    struct OstreeDeploymentVersion deployed_res;
    if (read_deployment_info(file, &deployed_res) != 0) {
      break;
    }

    if (booted != NULL) {
      struct OstreeDeploymentVersion booted_res;
      if (read_deployment_info(file, &booted_res) != 0) {
        break;
      }
      *booted = booted_res;
    }

    *deployed = deployed_res;
    return_code = 0;
  } while (0);

  fclose(file);
  if (return_code != 0) {
    printf("%s: Failed to read the deployment info from: %s\n", __FILE__, filename);
  }
  return return_code;
}
