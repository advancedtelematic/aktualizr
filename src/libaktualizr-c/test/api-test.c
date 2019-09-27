#include <stdio.h>
#include <stdlib.h>

#include "api-test-utils/api-test-utils.h"
#include "libaktualizr-c.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

int main(int argc, char **argv) {
  Aktualizr *a;
  Campaign *c;
  Updates *u;
  Target *t;
  int err;

  if (argc != 4) {
    fprintf(stderr, "Incorrect input params\nUsage:\n\t%s CONFIG_FILE FAKE_HTTP_ERVER_PATH UPTANE_GENERATOR_PATH\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  Run_fake_http_server(argv[2]);

  TemporaryDirectory *temp_dir = Get_temporary_directory();
  UptaneGenerator *g = Get_uptane_generator(argv[3]);

  const char *meta_dir = Get_temporary_directory_path(temp_dir);

  const char *gen_args[] = {"generate", "--path", meta_dir, "--correlationid", "abc123"};
  const char *image_args[] = {"image",        "--path",       meta_dir, "--filename", "tests/test_data/firmware.txt",
                              "--targetname", "firmware.txt", "--hwid", "primary_hw"};
  const char *add_target_args[] = {"addtarget", "--path",     meta_dir,   "--targetname",     "firmware.txt",
                                   "--hwid",    "primary_hw", "--serial", "CA:FE:A6:D2:84:9D"};
  const char *sign_args[] = {"signtargets", "--path", meta_dir, "--correlationid", "abc123"};

  Run_uptane_generator(g, gen_args, ARRAY_SIZE(gen_args));
  Run_uptane_generator(g, image_args, ARRAY_SIZE(image_args));
  Run_uptane_generator(g, add_target_args, ARRAY_SIZE(add_target_args));
  Run_uptane_generator(g, sign_args, ARRAY_SIZE(sign_args));

  Remove_temporary_directory(temp_dir);
  Remove_uptane_generator(g);

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
