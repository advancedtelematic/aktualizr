#include <gtest/gtest.h>

#include <boost/format.hpp>

#include "cert_provider_test.h"
#include "config/config.h"
#include "utilities/utils.h"

static boost::filesystem::path CERT_PROVIDER_PATH;
static boost::filesystem::path CREDENTIALS_PATH;

class AktualizrCertProviderTest : public ::testing::Test {
 protected:
  struct TestArgs {
    TestArgs(const TemporaryDirectory& tmp_dir, const std::string& cred_path_in)
        : test_dir{tmp_dir.PathString()}, credentials_path(tmp_dir.Path() / "credentials.zip") {
      boost::filesystem::copy_file(cred_path_in, credentials_path);
    }

    const std::string test_dir;
    const std::string fleet_ca_cert = "tests/test_data/CAcert.pem";
    const std::string fleet_ca_private_key = "tests/test_data/CApkey.pem";
    const boost::filesystem::path credentials_path;
  };

  TemporaryDirectory tmp_dir_;
  TestArgs test_args_{tmp_dir_, CREDENTIALS_PATH.string()};
  DeviceCredGenerator device_cred_gen_{CERT_PROVIDER_PATH.string()};
};

/**
 * Verifies that cert-provider works when given shared provisioning credentials
 * and the fleet CA and private key are not specified.
 *
 * - [x] Use shared provisioning credentials if fleet CA and private key are not provided
 *  - [x] Provision with shared credentials
 *  - [x] Read server root CA from p12
 *  - [x] Provide root CA if requested
 *  - [x] Provide server URL if requested
 */
TEST_F(AktualizrCertProviderTest, SharedCredProvisioning) {
  DeviceCredGenerator::ArgSet args;

  args.credentialFile = test_args_.credentials_path.string();
  args.localDir = test_args_.test_dir;
  args.provideRootCA.set();
  args.provideServerURL.set();

  device_cred_gen_.run(args);
  ASSERT_EQ(device_cred_gen_.lastExitCode(), 0) << device_cred_gen_.lastStdErr();

  DeviceCredGenerator::OutputPath device_cred_path(test_args_.test_dir);

  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.privateKeyFileFullPath))
      << device_cred_path.privateKeyFileFullPath;
  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.certFileFullPath)) << device_cred_path.certFileFullPath;

  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.serverRootCAFullPath))
      << device_cred_path.serverRootCAFullPath;
  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.gtwURLFileFullPath)) << device_cred_path.gtwURLFileFullPath;

  Process openssl("/usr/bin/openssl");

  openssl.run({"verify", "-verbose", "-CAfile", device_cred_path.serverRootCAFullPath.string(),
               device_cred_path.certFileFullPath.string()});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_EQ(openssl.lastStdOut(), str(boost::format("%1%: OK\n") % device_cred_path.certFileFullPath.string()));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc < 3) {
    std::cerr << "Two arguments are required: <path-to-cert-provider> <path-to-credentials>" << std::endl;
    return EXIT_FAILURE;
  }

  CERT_PROVIDER_PATH = argv[1];
  std::cout << "Path to the cert-provider executable: " << CERT_PROVIDER_PATH << std::endl;

  CREDENTIALS_PATH = argv[2];
  std::cout << "Path to the shared provisioning credentials: " << CREDENTIALS_PATH << std::endl;

  int test_run_res = RUN_ALL_TESTS();

  return test_run_res;
}
#endif
