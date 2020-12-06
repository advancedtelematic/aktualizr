#include <gtest/gtest.h>

#include <future>
#include <iostream>
#include <string>
#include <thread>

#include <boost/process.hpp>

#include "http/httpclient.h"
#include "image_repo.h"
#include "libaktualizr/config.h"
#include "logging/logging.h"
#include "package_manager/ostreemanager.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "uptane/imagerepository.h"

class TufRepoMock {
 public:
  TufRepoMock(const boost::filesystem::path& root_dir, std::string expires = "",
              std::string correlation_id = "corellatio-id")
      : repo_{root_dir, expires, correlation_id},
        port_{TestUtils::getFreePort()},
        url_{"http://localhost:" + port_},
        process_{"tests/fake_http_server/fake_test_server.py", port_, "-m", root_dir} {
    repo_.generateRepo(KeyType::kED25519);
    TestUtils::waitForServer(url_ + "/");
  }
  ~TufRepoMock() {
    process_.terminate();
    process_.wait_for(std::chrono::seconds(10));
  }

 public:
  const std::string& url() { return url_; }

  Uptane::Target add_target(const std::string& target_name, const std::string& hash, const std::string& hardware_id) {
    Delegation empty_delegetion{};
    Hash hash_obj{Hash::Type::kSha256, hash};
    Json::Value custom_json;
    custom_json["targetFormat"] = "OSTREE";

    repo_.addCustomImage(target_name, hash_obj, 0, hardware_id, "", empty_delegetion, custom_json);

    Json::Value target;
    target["length"] = 0;
    target["hashes"]["sha256"] = hash;
    target["custom"] = custom_json;

    return Uptane::Target(target_name, target);
  }

 private:
  ImageRepo repo_;
  std::string port_;
  std::string url_;
  boost::process::child process_;
};

class Treehub {
 public:
  Treehub(const std::string& root_dir)
      : root_dir_{root_dir},
        port_{TestUtils::getFreePort()},
        url_{"http://localhost:" + port_},
        process_{"tests/sota_tools/treehub_server.py",
                 std::string("-p"),
                 port_,
                 std::string("-d"),
                 root_dir,
                 std::string("-s0.5"),
                 std::string("--create"),
                 std::string("--system")} {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    TestUtils::waitForServer(url_ + "/");
  }

  ~Treehub() {
    process_.terminate();
    process_.wait_for(std::chrono::seconds(10));
  }

  const std::string& url() { return url_; }

  std::string getRev() {
    Process ostree_process{"ostree"};
    Process::Result result = ostree_process.run({"rev-parse", std::string("--repo"), root_dir_, "master"});
    if (std::get<0>(result) != 0) {
      throw std::runtime_error("Failed to get the current ostree revision in Treehub: " + std::get<2>(result));
    }
    auto res_rev = std::get<1>(result);
    boost::trim_if(res_rev, boost::is_any_of(" \t\r\n"));
    return res_rev;
  }

 private:
  const std::string root_dir_;
  std::string port_;
  std::string url_;
  boost::process::child process_;
};

class ComposeAppPackManMock : public OstreeManager {
 public:
  ComposeAppPackManMock(const PackageConfig& pconfig, const BootloaderConfig& bconfig,
                        const std::shared_ptr<INvStorage>& storage, const std::shared_ptr<HttpInterface>& http)
      : OstreeManager(pconfig, bconfig, storage, http) {
    sysroot_ = LoadSysroot(pconfig.sysroot);
  }

  std::string getCurrentHash() const override {
    g_autoptr(GPtrArray) deployments = nullptr;
    OstreeDeployment* deployment = nullptr;

    deployments = ostree_sysroot_get_deployments(sysroot_.get());
    if (deployments != nullptr && deployments->len > 0) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      deployment = static_cast<OstreeDeployment*>(deployments->pdata[0]);
    }
    auto hash = ostree_deployment_get_csum(deployment);
    return hash;
  }

  data::InstallationResult finalizeInstall(const Uptane::Target& target) override {
    auto cur_deployed_hash = getCurrentHash();

    if (target.sha256Hash() == cur_deployed_hash) {
      storage_->saveInstalledVersion("", target, InstalledVersionUpdateMode::kCurrent);
      return data::InstallationResult{data::ResultCode::Numeric::kOk, "Update has been successfully applied"};
    }
    return data::InstallationResult{data::ResultCode::Numeric::kInstallFailed, "Update has failed"};
  }

 private:
  GObjectUniquePtr<OstreeSysroot> sysroot_;
};

class AkliteMock {
 public:
  AkliteMock(const AkliteMock&) = delete;
  AkliteMock(const Config& config)
      : storage_{INvStorage::newStorage(config.storage)},
        key_manager_{std_::make_unique<KeyManager>(storage_, config.keymanagerConfig())},
        http_client_{std::make_shared<HttpClient>()},
        package_manager_{
            std::make_shared<ComposeAppPackManMock>(config.pacman, config.bootloader, storage_, http_client_)},
        fetcher_{std::make_shared<Uptane::Fetcher>(config, http_client_)} {
    finalizeIfNeeded();
  }
  ~AkliteMock() {}

 public:
  data::InstallationResult update() {
    image_repo_.updateMeta(*storage_, *fetcher_);
    image_repo_.checkMetaOffline(*storage_);

    std::shared_ptr<const Uptane::Targets> targets = image_repo_.getTargets();

    if (0 == targets->targets.size()) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    Uptane::Target target{targets->targets[0]};

    if (TargetStatus::kNotFound != package_manager_->verifyTarget(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    if (isTargetCurrent(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed,
                                      "Target has been already processed");
    }

    if (!package_manager_->fetchTarget(target, *fetcher_, *key_manager_, nullptr, nullptr)) {
      return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "Failed to download Target");
    }

    if (TargetStatus::kGood != package_manager_->verifyTarget(target)) {
      return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, "Target is invalid");
    }

    const auto install_result = package_manager_->install(target);
    if (install_result.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
      storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);
    } else if (install_result.result_code.num_code == data::ResultCode::Numeric::kOk) {
      storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
    }

    return install_result;
  }

  bool isTargetCurrent(const Uptane::Target& target) {
    const auto cur_target = package_manager_->getCurrent();

    if (cur_target.type() != target.type() || target.type() != "OSTREE") {
      LOG_ERROR << "Target formats mismatch: " << cur_target.type() << " != " << target.type();
      return false;
    }

    if (cur_target.length() != target.length()) {
      LOG_INFO << "Target names differ " << cur_target.filename() << " != " << target.filename();
      return false;
    }

    if (cur_target.filename() != target.filename()) {
      LOG_INFO << "Target names differ " << cur_target.filename() << " != " << target.filename();
      return false;
    }

    if (cur_target.sha256Hash() != target.sha256Hash()) {
      LOG_ERROR << "Target hashes differ " << cur_target.sha256Hash() << " != " << target.sha256Hash();
      return false;
    }

    return true;
  }

  data::InstallationResult finalizeIfNeeded() {
    boost::optional<Uptane::Target> pending_version;
    storage_->loadInstalledVersions("", nullptr, &pending_version);
    if (!!pending_version) {
      return package_manager_->finalizeInstall(*pending_version);
    }
    return data::InstallationResult{data::ResultCode::Numeric::kAlreadyProcessed, "Already installed"};
  }

 private:
  std::shared_ptr<INvStorage> storage_;
  std::unique_ptr<KeyManager> key_manager_;
  std::shared_ptr<HttpClient> http_client_;
  std::shared_ptr<PackageManagerInterface> package_manager_;
  std::shared_ptr<Uptane::Fetcher> fetcher_;
  Uptane::ImageRepository image_repo_;
};

class AkliteTest : public ::testing::Test {
 public:
  static std::string SysRootSrc;

 protected:
  AkliteTest()
      : tuf_repo_{test_dir_.Path() / "repo"},
        treehub_{(test_dir_.Path() / "treehub").string()},
        sysroot_path_{test_dir_.Path() / "sysroot"} {
    const auto sysroot_cmd = std::string("cp -r ") + SysRootSrc + std::string(" ") + sysroot_path_.string();
    if (0 != system(sysroot_cmd.c_str())) {
      throw std::runtime_error("Failed to copy a system rootfs");
    }

    conf_.uptane.repo_server = tufRepo().url() + "/repo";
    conf_.provision.primary_ecu_hardware_id = "primary_hw";
    conf_.pacman.type = PACKAGE_MANAGER_OSTREE;
    conf_.pacman.sysroot = sysrootPath();
    conf_.pacman.ostree_server = treehub().url();
    conf_.pacman.os = "dummy-os";
    conf_.storage.path = test_dir_.Path();
  }

  TufRepoMock& tufRepo() { return tuf_repo_; }
  Treehub& treehub() { return treehub_; }
  boost::filesystem::path& sysrootPath() { return sysroot_path_; }
  Config& conf() { return conf_; }

 private:
  TemporaryDirectory test_dir_;
  TufRepoMock tuf_repo_;
  Treehub treehub_;
  boost::filesystem::path sysroot_path_;
  Config conf_;
};

std::string AkliteTest::SysRootSrc;

/*
 * Test that mimics aktualizr-lite
 *
 * It makes use of libaktualizr's components and makes API calls to them in the way as aktualiz-lite
 * would do during its regular update cycle.
 */
TEST_F(AkliteTest, ostreeUpdate) {
  const auto target_to_install = tufRepo().add_target("target-01", treehub().getRev(), "primary_hw");
  {
    AkliteMock aklite{conf()};
    const auto update_result = aklite.update();
    ASSERT_EQ(update_result.result_code.num_code, data::ResultCode::Numeric::kNeedCompletion);
  }
  // reboot emulation by destroing and creating of a new AkliteMock instance
  {
    AkliteMock aklite{conf()};
    ASSERT_TRUE(aklite.isTargetCurrent(target_to_install));
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the test OSTree sysroot\n";
    return EXIT_FAILURE;
  }
  AkliteTest::SysRootSrc = argv[1];

  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
