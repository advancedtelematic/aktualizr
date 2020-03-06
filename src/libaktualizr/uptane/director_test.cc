#include <gtest/gtest.h>

#include "directorrepository.h"
#include "test_utils.h"
#include "utilities/utils.h"

boost::filesystem::path uptane_generator_path;

namespace Uptane {

/*
 * Verify that we correctly persist non-empty Targets metadata after receiving
 * subsequent Targets metadata that is empty.
 */
TEST(Director, EmptyTargets) {
  TemporaryDirectory meta_dir;

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString()});

  DirectorRepository director;
  EXPECT_TRUE(director.initRoot(Utils::readFile(meta_dir.Path() / "repo/director/root.json")));

  EXPECT_TRUE(director.verifyTargets(Utils::readFile(meta_dir.Path() / "repo/director/targets.json")));
  EXPECT_TRUE(director.targets.targets.empty());
  EXPECT_TRUE(director.latest_targets.targets.empty());

  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString()});

  EXPECT_TRUE(director.verifyTargets(Utils::readFile(meta_dir.Path() / "repo/director/targets.json")));
  EXPECT_EQ(director.targets.targets.size(), 1);
  EXPECT_EQ(director.targets.targets[0].filename(), "firmware.txt");
  EXPECT_EQ(director.targets.targets.size(), director.latest_targets.targets.size());

  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  EXPECT_TRUE(director.verifyTargets(Utils::readFile(meta_dir.Path() / "repo/director/targets.json")));
  EXPECT_EQ(director.targets.targets.size(), 1);
  EXPECT_EQ(director.targets.targets[0].filename(), "firmware.txt");
  EXPECT_TRUE(director.latest_targets.targets.empty());
}

}  // namespace Uptane

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
