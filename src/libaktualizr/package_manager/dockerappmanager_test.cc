#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include "config/config.h"
#include "http/httpclient.h"
#include "package_manager/dockerappmanager.h"
#include "package_manager/ostreemanager.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/fetcher.h"

static std::string repo_server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
static boost::filesystem::path test_sysroot;
static boost::filesystem::path uptane_gen;

static void progress_cb(const Uptane::Target& target, const std::string& description, unsigned int progress) {
  (void)description;
  LOG_INFO << "progress_cb " << target << " " << progress;
}

static std::unique_ptr<boost::process::child> create_repo(const boost::filesystem::path& repo_path) {
  std::string port = TestUtils::getFreePort();
  repo_server += port;
  auto p = std_::make_unique<boost::process::child>("src/libaktualizr/package_manager/dockerapp_test_repo.sh",
                                                    uptane_gen, repo_path, port);
  TestUtils::waitForServer(repo_server + "/");
  return p;
}

TEST(DockerAppManager, PackageManager_Factory_Good) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREEDOCKERAPP;
  config.pacman.sysroot = test_sysroot;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  auto pacman = PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
}

TEST(DockerAppManager, DockerAppConfig) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREEDOCKERAPP;
  config.pacman.sysroot = test_sysroot.string();
  config.pacman.ostree_server = treehub_server;
  config.pacman.extra["docker_apps_root"] = "apps-root";
  config.pacman.extra["docker_apps"] = "app1 app2";
  config.pacman.extra["docker_app_bin"] = "dab";
  config.pacman.extra["docker_compose_bin"] = "compose";

  DockerAppManagerConfig dacfg(config.pacman);
  ASSERT_TRUE(dacfg.docker_prune);
  ASSERT_EQ(2, dacfg.docker_apps.size());
  ASSERT_EQ("app1", dacfg.docker_apps[0]);
  ASSERT_EQ("app2", dacfg.docker_apps[1]);
  ASSERT_EQ("apps-root", dacfg.docker_apps_root);
  ASSERT_EQ("dab", dacfg.docker_app_bin);
  ASSERT_EQ("compose", dacfg.docker_compose_bin);

  config.pacman.extra["docker_prune"] = "0";
  dacfg = DockerAppManagerConfig(config.pacman);
  ASSERT_FALSE(dacfg.docker_prune);

  config.pacman.extra["docker_prune"] = "FALSE";
  dacfg = DockerAppManagerConfig(config.pacman);
  ASSERT_FALSE(dacfg.docker_prune);
}

TEST(DockerAppManager, DockerAppStandalone) {
  std::string sha = Utils::readFile(test_sysroot / "ostree/repo/refs/heads/ostree/1/1/0", true);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = sha;
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  target_json["custom"]["docker_apps"]["app1"]["filename"] = "foo.dockerapp";
  // Add a docker-app bundle entry to make sure we don't get confused
  target_json["custom"]["docker_apps"]["app1"]["uri"] = "http://foo.com";
  Uptane::Target target("pull", target_json);

  TemporaryDirectory temp_dir;
  auto repo = temp_dir.Path();
  auto repod = create_repo(repo);

  boost::filesystem::path apps_root = temp_dir / "docker_apps";

  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREEDOCKERAPP;
  config.pacman.sysroot = test_sysroot.string();
  config.pacman.ostree_server = treehub_server;
  config.pacman.extra["docker_apps_root"] = apps_root.string();
  config.pacman.extra["docker_apps"] = "app1 app2";
  config.pacman.extra["docker_app_bin"] = config.pacman.extra["docker_compose_bin"] =
      "src/libaktualizr/package_manager/docker_fake.sh";
  config.uptane.repo_server = repo_server + "/repo/repo";
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  // Create a fake "docker-app" that's not configured. We'll test below
  // to ensure its removed
  boost::filesystem::create_directories(apps_root / "delete-this-app");
  // This is app is configured, but not a part of the install Target, so
  // it should get removed below
  boost::filesystem::create_directories(apps_root / "app2");

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());
  auto client = std_::make_unique<SotaUptaneClient>(config, storage);
  ASSERT_NO_THROW(client->updateImageMeta());

  std::string targets = Utils::readFile(repo / "repo/repo/targets.json");
  LOG_INFO << "Repo targets " << targets;

  bool result = client->package_manager_->fetchTarget(target, *(client->uptane_fetcher), keys, progress_cb, nullptr);
  ASSERT_TRUE(result);

  auto hashes = std::vector<Hash>{
      Hash(Hash::Type::kSha256, "dfca385c923400228c8ddd3c2d572919985e48a9409a2d71dab33148017231c3"),
      Hash(Hash::Type::kSha512,
           "76b183d51f53613a450825afc6f984077b68ae7b321ba041a2b3871f3c25a9a20d964ad0b60352e5fdd09b78fd53879f4e3"
           "fa3dcc8335b26d3bbf455803d2ecb")};
  Uptane::Target app_target("foo.dockerapp", Uptane::EcuMap{}, hashes, 8);
  ASSERT_TRUE(storage->checkTargetFile(app_target));

  client->package_manager_->install(target);
  std::string content = Utils::readFile(apps_root / "app1/docker-compose.yml");
  ASSERT_EQ("DOCKER-APP RENDER OUTPUT\nfake contents of a docker app\n", content);

  // Make sure the unconfigured docker app has been removed:
  ASSERT_FALSE(boost::filesystem::exists(apps_root / "delete-this-app"));
  ASSERT_FALSE(boost::filesystem::exists(apps_root / "app2"));
  ASSERT_TRUE(boost::filesystem::exists(apps_root / "docker-compose-down-called"));

  setenv("DOCKER_APP_FAIL", "1", 1);
  result = client->package_manager_->fetchTarget(target, *(client->uptane_fetcher), keys, progress_cb, nullptr);
  ASSERT_FALSE(result);
}

TEST(DockerAppManager, DockerAppBundles) {
  // Define the target we want to update to
  unsetenv("DOCKER_APP_FAIL");  // Make sure this is cleared from previous test
  std::string sha = Utils::readFile(test_sysroot / "ostree/repo/refs/heads/ostree/1/1/0", true);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = sha;
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  target_json["custom"]["docker_apps"]["app1"]["uri"] = "hub.docker.io/user/hello@sha256:deadbeef";
  // Add a standalone entry to make sure we don't get confused
  target_json["custom"]["docker_apps"]["app1"]["filename"] = "foo.dockerapp";
  Uptane::Target target("pull", target_json);

  TemporaryDirectory temp_dir;

  // Insert a facke "docker" binary into our path
  boost::filesystem::path docker =
      boost::filesystem::system_complete("src/libaktualizr/package_manager/docker_fake.sh");
  ASSERT_EQ(0, symlink(docker.c_str(), (temp_dir.Path() / "docker").c_str()));
  std::string path(temp_dir.PathString() + ":" + getenv("PATH"));
  setenv("PATH", path.c_str(), 1);

  // Set up a SotaUptaneClient to test with
  boost::filesystem::path apps_root = temp_dir / "docker_apps";
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREEDOCKERAPP;
  config.pacman.sysroot = test_sysroot;
  config.pacman.extra["docker_apps_root"] = apps_root.string();
  config.pacman.extra["docker_apps"] = "app1 app2";
  config.pacman.extra["docker_compose_bin"] = "src/libaktualizr/package_manager/docker_fake.sh";
  config.pacman.ostree_server = treehub_server;
  config.uptane.repo_server = repo_server + "/repo/repo";
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  // Create a fake "docker-app" that's not configured. We'll test below
  // to ensure its removed
  // boost::filesystem::create_directories(apps_root / "delete-this-app");
  // This is app is configured, but not a part of the install Target, so
  // it should get removed below
  boost::filesystem::create_directories(apps_root / "app2");

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  storage->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
  KeyManager keys(storage, config.keymanagerConfig());
  auto client = std_::make_unique<SotaUptaneClient>(config, storage);

  ASSERT_TRUE(client->package_manager_->fetchTarget(target, *(client->uptane_fetcher), keys, progress_cb, nullptr));
  std::string content = Utils::readFile(temp_dir / "docker-app-pull");
  ASSERT_EQ("PULL CALLED app pull hub.docker.io/user/hello@sha256:deadbeef\n", content);

  client->package_manager_->install(target);
  content = Utils::readFile(apps_root / "app1/docker-compose.yml");
  ASSERT_EQ("FAKE CONTENT FOR IMAGE RENDER\nhub.docker.io/user/hello@sha256:deadbeef\n", content);

  // Make sure the unconfigured docker app has been removed:
  ASSERT_FALSE(boost::filesystem::exists(apps_root / "delete-this-app"));
  ASSERT_FALSE(boost::filesystem::exists(apps_root / "app2"));
  ASSERT_TRUE(boost::filesystem::exists(apps_root / "docker-compose-down-called"));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 3) {
    std::cerr << "Error: " << argv[0]
              << " requires the path to an OSTree sysroot and uptane-generator as an input argument.\n";
    return EXIT_FAILURE;
  }
  uptane_gen = argv[2];

  std::string port = TestUtils::getFreePort();
  treehub_server += port;
  boost::process::child server_process("tests/fake_http_server/fake_test_server.py", port);

  TemporaryDirectory temp_dir;
  // Utils::copyDir doesn't work here. Complaints about non existent symlink path
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + temp_dir.PathString()).c_str());
  if (r != 0) {
    return -1;
  }
  test_sysroot = (temp_dir.Path() / "ostree_repo").string();

  TestUtils::waitForServer(treehub_server + "/");

  return RUN_ALL_TESTS();
}
#endif
