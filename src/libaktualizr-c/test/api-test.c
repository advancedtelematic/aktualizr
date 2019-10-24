#include <stdio.h>
#include <stdlib.h>

#include "api-test-utils/api-test-utils.h"
#include "libaktualizr-c.h"

#define CLEANUP_AND_RETURN_FAILED \
  Stop_fake_http_server(server);  \
  return EXIT_FAILURE;

int main(int argc, char **argv) {
  Aktualizr *a;
  Campaign *c;
  Updates *u;
  Target *t;
  Config *cfg;
  FakeHttpServer *server;
  int err;

  if (argc != 3) {
    fprintf(stderr, "Incorrect input params\nUsage:\n\t%s FAKE_HTTP_SERVER_PATH META_DIR_PATH\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *serverPath = argv[1];
  const char *metaPath = argv[2];
  server = Run_fake_http_server(serverPath, metaPath);

  cfg = Get_test_config(/* storagePath = */ metaPath);
  a = Aktualizr_create_from_cfg(cfg);
  if (!a) {
    printf("Aktualizr_create_from_cfg failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  err = Aktualizr_initialize(a);
  if (err) {
    printf("Aktualizr_initialize failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  c = Aktualizr_campaigns_check(a);
  if (c == NULL) {
    printf("Aktualizr_campaigns_check returned NULL\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  printf("Accepting running campaign\n");
  err = Aktualizr_campaign_accept(a, c);
  if (err) {
    printf("Aktualizr_campaign_accept failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }
  Aktualizr_campaign_free(c);

  u = Aktualizr_updates_check(a);
  if (u == NULL) {
    printf("Aktualizr_updates_check returned NULL\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  size_t targets_num = Aktualizr_get_targets_num(u);
  if (targets_num == 0) {
    printf("Aktualizr_get_targets_num returned 0 targets\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  printf("Found new updates for %zu target(s)\n", targets_num);
  for (size_t i = 0; i < targets_num; i++) {
    t = Aktualizr_get_nth_target(u, i);
    char *name = Aktualizr_get_target_name(t);
    if (name == NULL) {
      printf("Aktualizr_get_target_name returned NULL\n");
      CLEANUP_AND_RETURN_FAILED;
    }
    printf("Downloading target %s\n", name);

    err = Aktualizr_download_target(a, t);
    if (err) {
      printf("Aktualizr_download_target failed\n");
      CLEANUP_AND_RETURN_FAILED;
    }

    printf("Installing...\n");
    err = Aktualizr_install_target(a, t);
    if (err) {
      printf("Aktualizr_install_target failed\n");
      CLEANUP_AND_RETURN_FAILED;
    }
    printf("Installation completed\n");

    void *handle = Aktualizr_open_stored_target(a, t);
    if (!handle) {
      printf("Aktualizr_open_stored_target failed\n");
      CLEANUP_AND_RETURN_FAILED;
    }

    const size_t bufSize = 1024;
    uint8_t *buf = malloc(bufSize);
    size_t size = Aktualizr_read_stored_target(handle, buf, bufSize);
    printf("Downloading target %s: extracted %li bytes (buffer size = %li), content:\n", name, (long int)size,
           (long int)bufSize);
    free(name);
    name = NULL;

    if (size == 0) {
      printf("Aktualizr_read_stored_target read 0 bytes\n");
      CLEANUP_AND_RETURN_FAILED;
    }

    for (size_t iBuf = 0; iBuf < size; ++iBuf) {
      printf("%c", buf[iBuf]);
    }
    if (size == bufSize) {
      printf(" ... (end of content skipped)");
    }
    free(buf);
    buf = NULL;

    err = Aktualizr_close_stored_target(handle);
    if (err) {
      printf("Aktualizr_close_stored_target failed\n");
      CLEANUP_AND_RETURN_FAILED;
    }
  }

#if 0
  err = Aktualizr_uptane_cycle(a);
  if (err) {
    printf("Aktualizr_uptane_cycle failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }
#endif

  err = Aktualizr_send_manifest(a, "({\"test_field\":\"test_value\"})");
  if (err) {
    printf("Aktualizr_send_manifest failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  err = Aktualizr_send_device_data(a);
  if (err) {
    printf("Aktualizr_send_device_data failed\n");
    CLEANUP_AND_RETURN_FAILED;
  }

  Aktualizr_destroy(a);

  Stop_fake_http_server(server);
  Remove_test_config(cfg);

  return EXIT_SUCCESS;
}
