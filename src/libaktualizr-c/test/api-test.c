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
    fprintf(stdout, "Found new updates for %zu target(s)\n", n);
    for (size_t i = 0; i < n; i++) {
      t = Aktualizr_get_nth_target(u, i);
      fprintf(stdout, "Downloading target %s\n", Aktualizr_get_target_name(t));
      Aktualizr_download_target(a, t);
      fprintf(stdout, "Installing...\n");
      Aktualizr_install_target(a, t);
      fprintf(stdout, "Installation completed\n");
    }
  }
#if 0
  err = Aktualizr_uptane_cycle(a);
  if (err) {
    return EXIT_FAILURE;
  }
#endif

  err = Aktualizr_send_manifest(a, R"({"test_field":"test_value"})");
  if (err) {
    return EXIT_FAILURE;
  }

  err = Aktualizr_send_device_data(a);
  if (err) {
    return EXIT_FAILURE;
  }

  // TODO: Remove hardcoded filename and content
  const char *const filename = "Software2_Version2";
  const char *const content =
      "{\"hashes\" : {"
      "\"sha256\" : \"6F0CF24ADE13FFF76E943C003413D85C3E497C984C95C1ECEA1C9731CA86F13C\","
      "\"sha512\" : \"\"},"
      "\"length\" : 2}";

  void *handle = Aktualizr_open_stored_target(a, filename, content);
  if (!handle) {
    return EXIT_FAILURE;
  }

  const size_t bufSize = 1024;
  uint8_t *buf = malloc(bufSize);
  size_t size = Aktualizr_read_stored_target(handle, buf, bufSize);
  fprintf(stdout, "Downloading target %s: extracted %li bytes, content: ", filename, (long int)size);
  for (size_t i = 0; i < bufSize; ++i)
    fprintf(stdout, "%c", buf[i]);
  free(buf);
  if (size == 0) {
    return EXIT_FAILURE;
  }

  err = Aktualizr_close_stored_target(handle);
  if (err) {
    return EXIT_FAILURE;
  }

  Aktualizr_destroy(a);

  return EXIT_SUCCESS;
}
