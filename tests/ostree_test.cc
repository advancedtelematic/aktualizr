#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include "config.h"
#include "fsstorage.h"
#include "ostree.h"
#include "types.h"
#include "utils.h"

boost::filesystem::path sysroot;

TEST(OstreeManager, PullBadUriNoCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.ostree_server = "bad-url";
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_private_key_path = "public.key";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  KeyManager keys(storage, config);
  keys.loadKeys();
  data::InstallOutcome result = OstreeManager::pull(config, keys, "hash");

  EXPECT_EQ(result.first, data::INSTALL_FAILED);
  EXPECT_EQ(result.second, "Failed to parse uri: bad-url");
}

TEST(OstreeManager, PullBadUriWithCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.ostree_server = "bad-url";
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_private_key_path = "public.key";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config);
  keys.loadKeys();
  data::InstallOutcome result = OstreeManager::pull(config, keys, "hash");

  EXPECT_EQ(result.first, data::INSTALL_FAILED);
  EXPECT_EQ(result.second, "Failed to parse uri: bad-url");
}

TEST(OstreeManager, InstallBadUri) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("branch-name-hash", target_json);
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_private_key_path = "public.key";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  OstreeManager ostree(config.pacman, storage);
  data::InstallOutcome result = ostree.install(target);
  EXPECT_EQ(result.first, data::INSTALL_FAILED);
  EXPECT_EQ(result.second, "Refspec 'hash' not found");
}

TEST(OstreeManager, BadSysroot) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = "sysroot-that-is-missing";
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  EXPECT_THROW(OstreeManager ostree(config.pacman, storage), std::runtime_error);
}

TEST(OstreeManager, ParseInstalledPackages) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.pacman.packages_file = "tests/test_data/package.manifest";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  OstreeManager ostree(config.pacman, storage);
  Json::Value packages = ostree.getInstalledPackages();
  EXPECT_EQ(packages[0]["name"], "vim");
  EXPECT_EQ(packages[0]["version"], "1.0");
  EXPECT_EQ(packages[1]["name"], "emacs");
  EXPECT_EQ(packages[1]["version"], "2.0");
  EXPECT_EQ(packages[2]["name"], "bash");
  EXPECT_EQ(packages[2]["version"], "1.1");
}

TEST(OstreeManager, AddRemoteNoCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.storage.path = temp_dir.Path();

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  KeyManager keys(storage, config);
  keys.loadKeys();

  OstreeRepo *repo = NULL;
  GError *error = NULL;
  boost::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.pacman.sysroot);
  EXPECT_TRUE(ostree_sysroot_get_repo(sysroot.get(), &repo, NULL, &error));
  EXPECT_TRUE(OstreeManager::addRemote(repo, config.pacman.ostree_server, keys));

  g_autofree char *url = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "url", NULL, &url, &error));
  EXPECT_EQ(url, config.pacman.ostree_server);

  gboolean out_gpg_verify;
  EXPECT_TRUE(ostree_repo_get_remote_boolean_option(repo, remote, "gpg-verify", FALSE, &out_gpg_verify, &error));

  g_autofree char *ostree_cert = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-cert-path", NULL, &ostree_cert, &error));
  EXPECT_EQ(ostree_cert, nullptr);

  g_autofree char *ostree_key = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-key-path", NULL, &ostree_key, &error));
  EXPECT_EQ(ostree_key, nullptr);

  g_autofree char *ostree_ca = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-ca-path", NULL, &ostree_ca, &error));
  EXPECT_EQ(ostree_ca, nullptr);

  g_object_unref(repo);
}

TEST(OstreeManager, AddRemoteWithCreds) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.storage.path = temp_dir.Path();

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config);
  keys.loadKeys();

  OstreeRepo *repo = NULL;
  GError *error = NULL;
  boost::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.pacman.sysroot);
  EXPECT_TRUE(ostree_sysroot_get_repo(sysroot.get(), &repo, NULL, &error));
  EXPECT_TRUE(OstreeManager::addRemote(repo, config.pacman.ostree_server, keys));

  g_autofree char *url = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "url", NULL, &url, &error));
  EXPECT_EQ(url, config.pacman.ostree_server);

  gboolean out_gpg_verify;
  EXPECT_TRUE(ostree_repo_get_remote_boolean_option(repo, remote, "gpg-verify", FALSE, &out_gpg_verify, &error));

  g_autofree char *ostree_cert = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-cert-path", NULL, &ostree_cert, &error));
  EXPECT_EQ(ostree_cert, keys.getCertFile());

  g_autofree char *ostree_key = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-client-key-path", NULL, &ostree_key, &error));
  EXPECT_EQ(ostree_key, keys.getPkeyFile());

  g_autofree char *ostree_ca = NULL;
  EXPECT_TRUE(ostree_repo_get_remote_option(repo, remote, "tls-ca-path", NULL, &ostree_ca, &error));
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
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
