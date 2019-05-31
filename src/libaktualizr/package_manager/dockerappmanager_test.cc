#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include "config/config.h"
#include "http/httpclient.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/fetcher.h"

static std::string repo_server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
static boost::filesystem::path test_sysroot;
static boost::filesystem::path akrepo;

static void progress_cb(const Uptane::Target& target, const std::string& description, unsigned int progress) {
  (void)description;
  LOG_INFO << "progress_cb " << target << " " << progress;
}

static std::unique_ptr<boost::process::child> create_repo(const boost::filesystem::path& repo_path) {
  std::string port = TestUtils::getFreePort();
  repo_server += port;
  auto p = std_::make_unique<boost::process::child>("src/libaktualizr/package_manager/dockerapp_test_repo.sh", akrepo,
                                                    repo_path, port);
  TestUtils::waitForServer(repo_server + "/");
  return p;
}

TEST(DockerAppManager, PackageManager_Factory_Good) {
  Config config;
  config.pacman.type = PackageManager::kOstreeDockerApp;
  config.pacman.sysroot = test_sysroot;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  auto pacman = PackageManagerFactory::makePackageManager(config.pacman, storage, nullptr, nullptr);
  EXPECT_TRUE(pacman);
}

TEST(DockerAppManager, DockerApp_Fetch) {
  std::string sha = Utils::readFile(test_sysroot / "ostree/repo/refs/heads/ostree/1/1/0", true);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = sha;
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  target_json["custom"]["docker_apps"]["app1"]["filename"] = "foo.dockerapp";
  Uptane::Target target("pull", target_json);

  TemporaryDirectory temp_dir;
  auto repo = temp_dir.Path();
  auto repod = create_repo(repo);

  Config config;
  config.pacman.type = PackageManager::kOstreeDockerApp;
  config.pacman.sysroot = test_sysroot;
  config.pacman.docker_apps_root = temp_dir / "docker_apps";
  config.pacman.docker_apps.push_back("app1");
  config.pacman.docker_app_bin = config.pacman.docker_compose_bin = "src/libaktualizr/package_manager/docker_fake.sh";
  config.pacman.ostree_server = treehub_server;
  config.uptane.repo_server = repo_server + "/repo/image";
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());
  auto http = std::make_shared<HttpClient>();
  auto client = SotaUptaneClient::newTestClient(config, storage, http, nullptr);
  ASSERT_TRUE(client->updateImagesMeta());

  std::string targets = Utils::readFile(repo / "repo/image/targets.json");
  LOG_INFO << "Repo targets " << targets;

  bool result = client->package_manager_->fetchTarget(target, *(client->uptane_fetcher), keys, progress_cb, nullptr);
  ASSERT_TRUE(result);

  auto hashes = std::vector<Uptane::Hash>{
      Uptane::Hash(Uptane::Hash::Type::kSha256, "dfca385c923400228c8ddd3c2d572919985e48a9409a2d71dab33148017231c3"),
      Uptane::Hash(Uptane::Hash::Type::kSha512,
                   "76b183d51f53613a450825afc6f984077b68ae7b321ba041a2b3871f3c25a9a20d964ad0b60352e5fdd09b78fd53879f4e3"
                   "fa3dcc8335b26d3bbf455803d2ecb")};
  Uptane::Target app_target("foo.dockerapp", hashes, 8);
  ASSERT_TRUE(storage->checkTargetFile(app_target));

  client->package_manager_->install(target);
  std::string content = Utils::readFile(config.pacman.docker_apps_root / "app1/docker-compose.yml");
  ASSERT_EQ("DOCKER-APP RENDER OUTPUT\nfake contents of a docker app\n", content);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 3) {
    std::cerr << "Error: " << argv[0]
              << " requires the path to an OSTree sysroot and aktualizr-repo as an input argument.\n";
    return EXIT_FAILURE;
  }
  akrepo = argv[2];

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
