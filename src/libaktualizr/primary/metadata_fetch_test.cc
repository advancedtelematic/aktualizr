#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

boost::filesystem::path uptane_generator_path;

class HttpFakeMetaCounter : public HttpFake {
 public:
  HttpFakeMetaCounter(const boost::filesystem::path &test_dir_in, const boost::filesystem::path &meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}

  HttpResponse get(const std::string &url, int64_t maxsize) override {
    if (url.find("director/1.root.json") != std::string::npos) {
      ++director_1root_count;
    }
    if (url.find("director/2.root.json") != std::string::npos) {
      ++director_2root_count;
    }
    if (url.find("director/targets.json") != std::string::npos) {
      ++director_targets_count;
    }
    if (url.find("repo/1.root.json") != std::string::npos) {
      ++image_1root_count;
    }
    if (url.find("repo/2.root.json") != std::string::npos) {
      ++image_2root_count;
    }
    if (url.find("repo/timestamp.json") != std::string::npos) {
      ++image_timestamp_count;
    }
    if (url.find("repo/snapshot.json") != std::string::npos) {
      ++image_snapshot_count;
    }
    if (url.find("repo/targets.json") != std::string::npos) {
      ++image_targets_count;
    }

    return HttpFake::get(url, maxsize);
  }

  int director_1root_count{0};
  int director_2root_count{0};
  int director_targets_count{0};
  int image_1root_count{0};
  int image_2root_count{0};
  int image_timestamp_count{0};
  int image_snapshot_count{0};
  int image_targets_count{0};
};

/*
 * Don't download Image repo metadata if Director reports no new targets. Don't
 * download Snapshot and Targets metadata from the Image repo if the Timestamp
 * indicates nothing has changed.
 */
TEST(Aktualizr, MetadataFetch) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpFakeMetaCounter>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();

  // No updates scheduled: only download Director Root and Targets metadata.
  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString()});

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);
  EXPECT_EQ(http->director_1root_count, 1);
  EXPECT_EQ(http->director_2root_count, 1);
  EXPECT_EQ(http->director_targets_count, 1);
  EXPECT_EQ(http->image_1root_count, 0);
  EXPECT_EQ(http->image_2root_count, 0);
  EXPECT_EQ(http->image_timestamp_count, 0);
  EXPECT_EQ(http->image_snapshot_count, 0);
  EXPECT_EQ(http->image_targets_count, 0);

  // Two images added, but only one update scheduled: all metadata objects
  // should be fetched once.
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware_name.txt",
                  "--targetname", "firmware_name.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"adddelegation", "--path", meta_dir.PathString(), "--dname", "role-abc", "--dpattern", "abc/*"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString()});

  update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  EXPECT_EQ(http->director_1root_count, 1);
  EXPECT_EQ(http->director_2root_count, 2);
  EXPECT_EQ(http->director_targets_count, 2);
  EXPECT_EQ(http->image_1root_count, 1);
  EXPECT_EQ(http->image_2root_count, 1);
  EXPECT_EQ(http->image_timestamp_count, 1);
  EXPECT_EQ(http->image_snapshot_count, 1);
  EXPECT_EQ(http->image_targets_count, 1);

  // Update scheduled with pre-existing image: no need to refetch Image repo
  // Snapshot or Targets metadata.
  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware_name.txt", "--hwid",
                  "primary_hw", "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString()});

  update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  EXPECT_EQ(http->director_1root_count, 1);
  EXPECT_EQ(http->director_2root_count, 3);
  EXPECT_EQ(http->director_targets_count, 3);
  EXPECT_EQ(http->image_1root_count, 1);
  EXPECT_EQ(http->image_2root_count, 2);
  EXPECT_EQ(http->image_timestamp_count, 2);
  EXPECT_EQ(http->image_snapshot_count, 1);
  EXPECT_EQ(http->image_targets_count, 1);

  // Delegation added to an existing delegation; update scheduled with
  // pre-existing image: Snapshot must be refetched, but Targets are unchanged.
  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"adddelegation", "--path", meta_dir.PathString(), "--dname", "role-def", "--dpattern", "def/*",
                  "--dparent", "role-abc"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString()});

  update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  EXPECT_EQ(http->director_1root_count, 1);
  EXPECT_EQ(http->director_2root_count, 4);
  EXPECT_EQ(http->director_targets_count, 4);
  EXPECT_EQ(http->image_1root_count, 1);
  EXPECT_EQ(http->image_2root_count, 3);
  EXPECT_EQ(http->image_timestamp_count, 3);
  EXPECT_EQ(http->image_snapshot_count, 2);
  EXPECT_EQ(http->image_targets_count, 1);
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
