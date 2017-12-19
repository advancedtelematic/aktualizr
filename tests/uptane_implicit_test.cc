#include <gtest/gtest.h>

#include <string>

#include <boost/filesystem.hpp>

#include "fsstorage.h"
#include "httpfake.h"
#include "logger.h"
#include "sotauptaneclient.h"
#include "uptane/uptanerepository.h"

const std::string implicit_test_dir = "tests/test_implicit";

/**
 * \verify{\tst{185}} Verify that when using implicit provisioning, aktualizr
 * halts if credentials are not available.
 */
TEST(UptaneImplicit, ImplicitFailure) {
  boost::filesystem::remove_all(implicit_test_dir);
  Config config;
  config.uptane.device_id = "device_id";

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "/";
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.postUpdateValues();

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(uptane.initialize());
}

/**
 * \verify{\tst{187}} Verfiy that aktualizr halts when provided incomplete
 * implicit provisioning credentials.
 */
TEST(UptaneImplicit, ImplicitIncomplete) {
  boost::filesystem::remove_all(implicit_test_dir);
  Config config;
  config.storage.path = implicit_test_dir;
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  FSStorage storage(config.storage);
  HttpFake http(implicit_test_dir);
  Uptane::Repository uptane(config, storage, http);

  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", implicit_test_dir + "/ca.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", implicit_test_dir + "/client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", implicit_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", implicit_test_dir + "/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", implicit_test_dir + "/client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", implicit_test_dir + "/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", implicit_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
  boost::filesystem::create_directory(implicit_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", implicit_test_dir + "/client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", implicit_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(implicit_test_dir);
}

/**
 * \verify{\tst{186}} Verify that aktualizr can implicitly provision with
 * provided credentials.
 */
TEST(UptaneImplicit, ImplicitProvision) {
  boost::filesystem::remove_all(implicit_test_dir);
  Config config;
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir / "client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir / "pkey.pem");
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
