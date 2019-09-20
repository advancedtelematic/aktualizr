#define _GNU_SOURCE
#include <dlfcn.h>
#include <ostree.h>
#include <stdio.h>

/**
 * @brief This mock allows the ostree package manager usage on non-ostree booted environment, e.g. local host
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

static OstreeDeployment* ostree_sysroot_get_booted_deployment_from_file(const char* filename);

OstreeDeployment* ostree_sysroot_get_booted_deployment(OstreeSysroot* self) {
  OstreeDeployment* (*orig)(OstreeSysroot*) = (OstreeDeployment * (*)(OstreeSysroot*))(dlsym(RTLD_NEXT, __func__));

  const char* deployment_version_file = getenv(OSTREE_DEPLOYMENT_VERSION_FILE);
  OstreeDeployment* deployment_from_file = NULL;

  if (deployment_version_file != NULL &&
      (deployment_from_file = ostree_sysroot_get_booted_deployment_from_file(deployment_version_file)) != NULL) {
    return deployment_from_file;
  }

  return orig(self);
}

struct OstreeDeployment {
  int serial;
  char rev[255];
};

static int read_deployment_info(FILE* file, struct OstreeDeployment* deployment) {
  struct OstreeDeployment deployment_res;

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

static int get_ostree_deployment_info(const char* filename, struct OstreeDeployment* deployed,
                                      struct OstreeDeployment* booted) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    printf("\n>>>>>>>>>> Failed to open the file: %s\n\n", filename);
    return -1;
  }

  int return_code = -1;

  do {
    struct OstreeDeployment deployed_res;
    if (read_deployment_info(file, &deployed_res) != 0) {
      break;
    }

    if (booted != NULL) {
      struct OstreeDeployment booted_res;
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

static struct OstreeDeployment g_deployed;

const char* ostree_deployment_get_csum(OstreeDeployment* self) {
  const char* (*orig)(OstreeDeployment*) = (const char* (*)(OstreeDeployment*))(dlsym(RTLD_NEXT, __func__));

  const char* deployment_version_file = NULL;

  if ((deployment_version_file = getenv(OSTREE_DEPLOYMENT_VERSION_FILE)) != NULL) {
    if (get_ostree_deployment_info(deployment_version_file, &g_deployed, NULL) != 0) {
      return NULL;
    }

    return g_deployed.rev;
  }

  return orig(self);
}

OstreeDeployment* ostree_sysroot_get_booted_deployment_from_file(const char* filename) {
  struct OstreeDeployment deployed;
  struct OstreeDeployment booted;

  if (get_ostree_deployment_info(filename, &deployed, &booted) != 0) {
    return NULL;
  }

  OstreeDeployment* new_depl =
      ostree_deployment_new(0, OSTREE_DEPLOYMENT_OS_NAME, deployed.rev, deployed.serial, booted.rev, booted.serial);
  return new_depl;
}
