#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

boost::filesystem::path uptane_generator_path;

/*
 * Detect a mismatch in the Targets metadata from Director and Image repo.
 */
TEST(Aktualizr, HardwareMismatch) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "", meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  // Note bad hardware ID in the image repo metadata. If we were to use an
  // invalid value in the Director, it gets rejected even earlier.
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "hw-mismatch", "--url", "test-url"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString()});

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kError);
  EXPECT_EQ(update_result.message, "Target mismatch.");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the uptane-generator utility\n";
    return EXIT_FAILURE;
  }
  uptane_generator_path = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
