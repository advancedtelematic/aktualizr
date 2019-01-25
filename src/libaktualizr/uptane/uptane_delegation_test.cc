#include <gtest/gtest.h>

#include <string>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "config/config.h"
#include "httpfake.h"
#include "primary/aktualizr.h"
#include "primary/events.h"
#include "uptane_test_common.h"

boost::filesystem::path uptane_repos_dir;

class HttpFakeDelegationBasic : public HttpFake {
 public:
  HttpFakeDelegationBasic(const boost::filesystem::path& test_dir_in)
      : HttpFake(test_dir_in, "", uptane_repos_dir / "delegation_basic") {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (const Json::Value& event : data) {
      ++events_seen;
      EXPECT_EQ(event["event"]["correlationId"].asString(), "");
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  unsigned int events_seen{0};
};

/* Correlation ID is empty if none was provided in targets metadata. */
TEST(Delegation, Basic) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeDelegationBasic>(temp_dir.Path());

  // scope `Aktualizr` object, so that the ReportQueue flushes its events before
  // we count them at the end
  {
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    result::Install install_result = aktualizr.Install(download_result.updates).get();
    for (const auto& r : install_result.ecu_reports) {
      EXPECT_EQ(r.install_res.result_code.num_code, data::ResultCode::Numeric::kOk);
    }
  }

  EXPECT_EQ(http->events_seen, 8);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the base directory of uptane repos.\n";
    return EXIT_FAILURE;
  }
  uptane_repos_dir = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
