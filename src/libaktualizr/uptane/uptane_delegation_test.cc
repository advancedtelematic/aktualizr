#include <gtest/gtest.h>

#include <string>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include <libaktualizr/config.h>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include <libaktualizr/events.h>
#include "uptane_test_common.h"

boost::filesystem::path uptane_generator_path;

void delegation_basic(const boost::filesystem::path& delegation_path, bool revoke) {
  std::string output;
  std::string cmd = "tests/uptane_repo_generation/delegation_basic.sh " + uptane_generator_path.string() + " " +
                    delegation_path.string();
  if (revoke) {
    cmd += " revoke";
  }
  int retval = Utils::shell(cmd, &output, true);
  EXPECT_EQ(retval, EXIT_SUCCESS) << output;
}

void delegation_nested(const boost::filesystem::path& delegation_path, bool revoke) {
  std::string output;
  std::string cmd = "tests/uptane_repo_generation/delegation_nested.sh " + uptane_generator_path.string() + " " +
                    delegation_path.string();
  if (revoke) {
    cmd += " revoke";
  }
  int retval = Utils::shell(cmd, &output, true);
  EXPECT_EQ(retval, EXIT_SUCCESS) << output;
}

class HttpFakeDelegation : public HttpFake {
 public:
  HttpFakeDelegation(const boost::filesystem::path& test_dir_in)
      : HttpFake(test_dir_in, "", test_dir_in / "delegation_test/repo") {}

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

/* Validate first-order target delegations.
 * Search first-order delegations.
 * Correlation ID is empty if none was provided in Targets metadata. */
TEST(Delegation, Basic) {
  for (auto generate_fun : {delegation_basic, delegation_nested}) {
    TemporaryDirectory temp_dir;
    auto delegation_path = temp_dir.Path() / "delegation_test";
    generate_fun(delegation_path, false);
    auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());

    // scope `Aktualizr` object, so that the ReportQueue flushes its events before
    // we count them at the end
    {
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

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
}

TEST(Delegation, RevokeAfterCheckUpdates) {
  for (auto generate_fun : {delegation_basic, delegation_nested}) {
    TemporaryDirectory temp_dir;
    auto delegation_path = temp_dir.Path() / "delegation_test";
    generate_fun(delegation_path, false);
    {
      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
      aktualizr.Initialize();

      result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
      EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
      EXPECT_EQ(update_result.updates.size(), 2);
    }
    // Revoke delegation after CheckUpdates() and test if we can properly handle it.
    {
      generate_fun(delegation_path, true);
      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
      aktualizr.Initialize();

      auto update_result = aktualizr.CheckUpdates().get();
      EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
      EXPECT_EQ(update_result.updates.size(), 1);

      result::Download download_result = aktualizr.Download(update_result.updates).get();
      EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

      result::Install install_result = aktualizr.Install(download_result.updates).get();
      for (const auto& r : install_result.ecu_reports) {
        EXPECT_EQ(r.install_res.result_code.num_code, data::ResultCode::Numeric::kOk);
      }
    }
  }
}

TEST(Delegation, RevokeAfterDownload) {
  for (auto generate_fun : {delegation_basic, delegation_nested}) {
    TemporaryDirectory temp_dir;
    auto delegation_path = temp_dir.Path() / "delegation_test";
    generate_fun(delegation_path, false);
    {
      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
      aktualizr.Initialize();

      result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
      EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

      result::Download download_result = aktualizr.Download(update_result.updates).get();
      EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);
    }
    // Revoke delegation after Download() and test if we can properly handle it
    {
      generate_fun(delegation_path, true);

      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
      aktualizr.Initialize();

      auto update_result = aktualizr.CheckUpdates().get();
      EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
      EXPECT_EQ(update_result.updates.size(), 1);

      result::Download download_result = aktualizr.Download(update_result.updates).get();
      EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

      result::Install install_result = aktualizr.Install(download_result.updates).get();
      for (const auto& r : install_result.ecu_reports) {
        EXPECT_EQ(r.install_res.result_code.num_code, data::ResultCode::Numeric::kOk);
      }
    }
  }
}

TEST(Delegation, RevokeAfterInstall) {
  for (auto generate_fun : {delegation_basic, delegation_nested}) {
    TemporaryDirectory temp_dir;
    auto delegation_path = temp_dir.Path() / "delegation_test";
    generate_fun(delegation_path, false);
    {
      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
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
    // Revoke delegation after Install() and test if can properly CheckUpdates again
    {
      generate_fun(delegation_path, true);

      auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());
      Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
      auto storage = INvStorage::newStorage(conf.storage);
      UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
      aktualizr.Initialize();

      auto update_result = aktualizr.CheckUpdates().get();
      EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);
    }
  }
}

/* Iterate over targets in delegation tree */
TEST(Delegation, IterateAll) {
  TemporaryDirectory temp_dir;
  auto delegation_path = temp_dir.Path() / "delegation_test";
  delegation_nested(delegation_path, false);
  auto http = std::make_shared<HttpFakeDelegation>(temp_dir.Path());

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  aktualizr.Initialize();
  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

  std::vector<std::string> expected_target_names = {"primary.txt", "abracadabra", "abc/secondary.txt", "abc/target0",
                                                    "abc/target1", "abc/target2", "bcd/target0",       "cde/target0",
                                                    "cde/target1", "def/target0"};
  for (auto& target : aktualizr.uptane_client()->allTargets()) {
    EXPECT_EQ(target.filename(), expected_target_names[0]);
    expected_target_names.erase(expected_target_names.begin());
  }

  EXPECT_TRUE(expected_target_names.empty());
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
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
