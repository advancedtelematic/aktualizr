#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "libaktualizr/config.h"
#include "package_manager/ostreemanager.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

boost::filesystem::path test_sysroot;

/* Reject bad OSTree server URIs. */
TEST(OstreeManager, PullBadUriNoCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.ostree_server = "bad-url";
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();
  Json::Value target_json_test;
  target_json_test["hashes"]["sha256"] = "some_hash";
  target_json_test["length"] = 0;
  Uptane::Target target_test("test.deb", target_json_test);
  data::InstallationResult result =
      OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target_test);

  EXPECT_EQ(result.result_code.num_code, data::ResultCode::Numeric::kInstallFailed);
  EXPECT_EQ(result.description, "Failed to parse uri: bad-url");
}

/* Reject bad OSTree server URIs. */
TEST(OstreeManager, PullBadUriWithCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.ostree_server = "bad-url";
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();
  Json::Value target_json_test;
  target_json_test["hashes"]["sha256"] = "some_hash";
  target_json_test["length"] = 0;
  Uptane::Target target_test("test.deb", target_json_test);
  data::InstallationResult result =
      OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target_test);

  EXPECT_EQ(result.result_code.num_code, data::ResultCode::Numeric::kInstallFailed);
  EXPECT_EQ(result.description, "Failed to parse uri: bad-url");
}

/* Reject bad OSTree server URIs. */
TEST(OstreeManager, InstallBadUri) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("branch-name-hash", target_json);
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  OstreeManager ostree(config.pacman, config.bootloader, storage, nullptr);
  data::InstallationResult result = ostree.install(target);
  EXPECT_EQ(result.result_code.num_code, data::ResultCode::Numeric::kInstallFailed);
  EXPECT_EQ(result.description, "Refspec 'hash' not found");
}

/* Abort if the OSTree sysroot is invalid. */
TEST(OstreeManager, BadSysroot) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = "sysroot-that-is-missing";
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  EXPECT_THROW(OstreeManager ostree(config.pacman, config.bootloader, storage, nullptr), std::runtime_error);
}

/* Parse a provided list of installed packages. */
TEST(OstreeManager, ParseInstalledPackages) {
  TemporaryDirectory temp_dir;
  boost::filesystem::path packages_file = temp_dir / "package.manifest";

  std::string content;
  {
    content += "vim 1.0\n";
    content += "emacs 2.0\n";
    content += "bash 1.1\n";
  }
  Utils::writeFile(packages_file, content);

  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.pacman.packages_file = packages_file;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  OstreeManager ostree(config.pacman, config.bootloader, storage, nullptr);
  Json::Value packages = ostree.getInstalledPackages();
  EXPECT_EQ(packages[0]["name"].asString(), "vim");
  EXPECT_EQ(packages[0]["version"].asString(), "1.0");
  EXPECT_EQ(packages[1]["name"].asString(), "emacs");
  EXPECT_EQ(packages[1]["version"].asString(), "2.0");
  EXPECT_EQ(packages[2]["name"].asString(), "bash");
  EXPECT_EQ(packages[2]["version"].asString(), "1.1");
}

/* Communicate with a remote OSTree server without credentials. */
TEST(OstreeManager, AddRemoteNoCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();

  OstreeRepo *repo = nullptr;
  GError *error = nullptr;
  std::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.pacman.sysroot);
  EXPECT_TRUE(ostree_sysroot_get_repo(sysroot.get(), &repo, nullptr, &error));
  EXPECT_TRUE(OstreeManager::addRemote(repo, config.pacman.ostree_server, keys));

  g_autofree char *url = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "url", nullptr, &url, &error));
  EXPECT_EQ(url, config.pacman.ostree_server);

  gboolean out_gpg_verify;
  EXPECT_TRUE(ostree_repo_get_remote_boolean_option(repo, remote, "gpg-verify", FALSE, &out_gpg_verify, &error));

  g_autofree char *ostree_cert = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-cert-path", nullptr, &ostree_cert, &error));
  EXPECT_EQ(ostree_cert, nullptr);

  g_autofree char *ostree_key = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-key-path", nullptr, &ostree_key, &error));
  EXPECT_EQ(ostree_key, nullptr);

  g_autofree char *ostree_ca = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-ca-path", nullptr, &ostree_ca, &error));
  EXPECT_EQ(ostree_ca, nullptr);

  g_object_unref(repo);
}

/* Communicate with a remote OSTree server with credentials. */
TEST(OstreeManager, AddRemoteWithCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();

  OstreeRepo *repo = nullptr;
  GError *error = nullptr;
  std::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.pacman.sysroot);
  EXPECT_TRUE(ostree_sysroot_get_repo(sysroot.get(), &repo, nullptr, &error));
  EXPECT_TRUE(OstreeManager::addRemote(repo, config.pacman.ostree_server, keys));

  g_autofree char *url = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "url", nullptr, &url, &error));
  EXPECT_EQ(url, config.pacman.ostree_server);

  gboolean out_gpg_verify;
  EXPECT_TRUE(ostree_repo_get_remote_boolean_option(repo, remote, "gpg-verify", FALSE, &out_gpg_verify, &error));

  g_autofree char *ostree_cert = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-cert-path", nullptr, &ostree_cert, &error));
  EXPECT_EQ(ostree_cert, keys.getCertFile());

  g_autofree char *ostree_key = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-key-path", nullptr, &ostree_key, &error));
  EXPECT_EQ(ostree_key, keys.getPkeyFile());

  g_autofree char *ostree_ca = nullptr;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-ca-path", nullptr, &ostree_ca, &error));
  EXPECT_EQ(ostree_ca, keys.getCaFile());

  g_object_unref(repo);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }

  TemporaryDirectory temp_sysroot;
  test_sysroot = temp_sysroot / "sysroot";
  // uses cp, as boost doesn't like to copy bad symlinks
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + test_sysroot.string()).c_str());
  if (r != 0) {
    return -1;
  }

  return RUN_ALL_TESTS();
}
#endif
