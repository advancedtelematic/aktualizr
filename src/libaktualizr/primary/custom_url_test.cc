#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "primary/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

boost::filesystem::path aktualizr_repo_path;

class HttpCheckUrl : public HttpFake {
 public:
  HttpCheckUrl(const boost::filesystem::path &test_dir_in, const boost::filesystem::path &meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}
  HttpResponse download(const std::string &url, curl_write_callback write_cb, curl_xferinfo_callback progress_cb,
                        void *userp, curl_off_t from) override {
    (void)write_cb;
    (void)progress_cb;
    (void)userp;
    (void)from;

    url_ = url;
    return HttpResponse("", 200, CURLE_OK, "");
  }

  std::string url_;
};

/*
 * If the URL from the Director is unset, but the URL from the Images repo is
 * set, use that.
 */
TEST(Aktualizr, ImagesCustomUrl) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpCheckUrl>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process akt_repo(aktualizr_repo_path.string());
  akt_repo.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  akt_repo.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt", "--targetname",
                "firmware.txt", "--hwid", "primary_hw", "--url", "test-url"});
  akt_repo.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                "--serial", "CA:FE:A6:D2:84:9D"});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  // Work around inconsistent directory naming.
  Utils::copyDir(meta_dir.Path() / "repo/image", meta_dir.Path() / "repo/repo");

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();
  EXPECT_EQ(http->url_, "");

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

  result::Download download_result = aktualizr.Download(update_result.updates).get();
  EXPECT_EQ(http->url_, "test-url");
}

/*
 * If the URL is set by both the Director and Images repo, use the version from
 * the Director.
 */
TEST(Aktualizr, BothCustomUrl) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpCheckUrl>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process akt_repo(aktualizr_repo_path.string());
  akt_repo.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  akt_repo.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt", "--targetname",
                "firmware.txt", "--hwid", "primary_hw", "--url", "test-url2"});
  akt_repo.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                "--serial", "CA:FE:A6:D2:84:9D", "--url", "test-url3"});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  // Work around inconsistent directory naming.
  Utils::copyDir(meta_dir.Path() / "repo/image", meta_dir.Path() / "repo/repo");

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();
  EXPECT_EQ(http->url_, "");

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

  result::Download download_result = aktualizr.Download(update_result.updates).get();
  EXPECT_EQ(http->url_, "test-url3");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the aktualizr-repo utility\n";
    return EXIT_FAILURE;
  }
  aktualizr_repo_path = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
