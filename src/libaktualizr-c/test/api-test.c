#include <stdio.h>
#include <stdlib.h>

#include "libaktualizr-c.h"

int main(int argc, char **argv) {
  Aktualizr *a;
  Campaign *c;
  Updates *u;
  Target *t;
  int err;

  if (argc != 2) {
    fprintf(stderr, "Missing config file\nUsage:\n\t%s CONFIG_FILE\n", argv[0]);
    return EXIT_FAILURE;
  }

  a = Aktualizr_create(argv[1]);
  if (!a) {
    return EXIT_FAILURE;
  }

  err = Aktualizr_initialize(a);
  if (err) {
    return EXIT_FAILURE;
  }

  c = Aktualizr_campaigns_check(a);
  if (c) {
    fprintf(stdout, "Accepting running campaign\n");
    err = Aktualizr_campaign_accept(a, c);
    Aktualizr_campaign_free(c);
    if (err) {
      return EXIT_FAILURE;
    }
  }

  u = Aktualizr_updates_check(a);
  if (u) {
    size_t n = Aktualizr_get_targets_num(u);
    printf("Found new updates for %zu target(s)\n", n);
    for (size_t i = 0; i < n; i++) {
      t = Aktualizr_get_nth_target(u, i);
      char *name = Aktualizr_get_target_name(t);
      printf("Downloading target %s\n", name);
      free(name);
      name = NULL;

      Aktualizr_download_target(a, t);
      printf("Installing...\n");

      Aktualizr_install_target(a, t);
      printf("Installation completed\n");

      void *handle = Aktualizr_open_stored_target(a, t);
      if (!handle) {
        return EXIT_FAILURE;
      }

      const size_t bufSize = 1024;
      uint8_t *buf = malloc(bufSize);
      size_t size = Aktualizr_read_stored_target(handle, buf, bufSize);
      name = Aktualizr_get_target_name(t);
      printf("Downloading target %s: extracted %li bytes (buffer size = %li), content:\n", name, (long int)size,
             (long int)bufSize);
      free(name);
      name = NULL;

      for (size_t iBuf = 0; iBuf < size; ++iBuf) {
        printf("%c", buf[iBuf]);
      }
      if (size == bufSize) {
        printf(" ... (end of content skipped)");
      }
      free(buf);
      buf = NULL;
      if (size == 0) {
        return EXIT_FAILURE;
      }

      err = Aktualizr_close_stored_target(handle);
      if (err) {
        return EXIT_FAILURE;
      }
    }
  }
#if 0
  err = Aktualizr_uptane_cycle(a);
  if (err) {
    return EXIT_FAILURE;
  }
#endif

  err = Aktualizr_send_manifest(a, "({\"test_field\":\"test_value\"})");
  if (err) {
    return EXIT_FAILURE;
  }

  err = Aktualizr_send_device_data(a);
  if (err) {
    return EXIT_FAILURE;
  }

  Aktualizr_destroy(a);

  return EXIT_SUCCESS;
}
