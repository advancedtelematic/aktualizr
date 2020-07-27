#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

boost::filesystem::path uptane_generator_path;

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
 * If the URL from the Director is unset, but the URL from the Image repo is
 * set, use that.
 */
TEST(Aktualizr, ImageCustomUrl) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpCheckUrl>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw", "--url", "test-url"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

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
 * If the URL is set by both the Director and Image repo, use the version from
 * the Director.
 */
TEST(Aktualizr, BothCustomUrl) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpCheckUrl>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw", "--url", "test-url2"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D", "--url", "test-url3"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

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
