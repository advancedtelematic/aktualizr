/**
 * \file
 */

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/smart_ptr/make_shared.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

/**
 * \verify{\tst{185}} Verify that when using implicit provisioning, aktualizr
 * halts if credentials are not available.
 */
TEST(UptaneImplicit, ImplicitFailure) {
  Config config;
  config.uptane.device_id = "device_id";

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(uptane.initialize());
}

/**
 * \verify{\tst{187}} Verfiy that aktualizr halts when provided incomplete
 * implicit provisioning credentials.
 */
TEST(UptaneImplicit, ImplicitIncomplete) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  auto storage = INvStorage::newStorage(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);

  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(temp_dir.Path());
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(temp_dir.Path());
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(temp_dir.Path());
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(temp_dir.Path());
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(temp_dir.Path());
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
  EXPECT_FALSE(uptane.initialize());
}

/**
 * \verify{\tst{186}} Verify that aktualizr can implicitly provision with
 * provided credentials.
 */
TEST(UptaneImplicit, ImplicitProvision) {
  Config config;
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir / "client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir / "pkey.pem");
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";

  auto storage = INvStorage::newStorage(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
