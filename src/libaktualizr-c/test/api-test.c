#include <stdio.h>
#include <stdlib.h>

#include "libaktualizr-c.h"

int main(int argc, char **argv) {
  Aktualizr *a;
  Campaign *c;
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

  c = Aktualizr_campaign_check(a);
  if (c) {
    printf("Accepting running campaign\n");
    err = Aktualizr_campaign_accept(a, c);
    Aktualizr_campaign_free(c);
    if (err) {
      return EXIT_FAILURE;
    }
  }

  err = Aktualizr_uptane_cycle(a);
  if (err) {
    return EXIT_FAILURE;
  }

  Aktualizr_destroy(a);

  return EXIT_SUCCESS;
}
