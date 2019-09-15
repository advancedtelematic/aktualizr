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
    printf("Accepting running campaign\n");
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
      printf("Downloading target %s\n", Aktualizr_get_target_name(t));
      Aktualizr_download_target(a, t);
      printf("Installing...\n");
      Aktualizr_install_target(a, t);
      printf("Installation completed\n");
    }
  }
#if 0
  err = Aktualizr_uptane_cycle(a);
  if (err) {
    return EXIT_FAILURE;
  }
#endif
  Aktualizr_destroy(a);

  return EXIT_SUCCESS;
}
