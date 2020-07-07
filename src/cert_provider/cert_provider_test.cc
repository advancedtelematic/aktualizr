#include <gtest/gtest.h>

#include <boost/format.hpp>

#include "cert_provider_test.h"
#include "crypto/crypto.h"
#include "libaktualizr/config.h"
#include "utilities/utils.h"

static boost::filesystem::path CERT_PROVIDER_PATH;

class AktualizrCertProviderTest : public ::testing::Test {
 protected:
  struct TestArgs {
    TestArgs(const TemporaryDirectory& tmp_dir) : test_dir{tmp_dir.PathString()} {}

    const std::string test_dir;
    const std::string fleet_ca_cert = "tests/test_data/CAcert.pem";
    const std::string fleet_ca_private_key = "tests/test_data/CApkey.pem";
  };

  class Cert {
   public:
    Cert(const std::string& cert_file_path) {
      fp_ = fopen(cert_file_path.c_str(), "r");
      if (!fp_) {
        throw std::invalid_argument("Cannot open the specified cert file: " + cert_file_path);
      }

      cert_ = PEM_read_X509(fp_, NULL, NULL, NULL);
      if (!cert_) {
        fclose(fp_);
        throw std::runtime_error("Failed to read the cert file: " + cert_file_path);
      }
    }

    ~Cert() {
      X509_free(cert_);
      fclose(fp_);
    }

    std::string getSubjectItemValue(int itemID) {
      const uint32_t SUBJECT_ITEM_MAX_LENGTH = 2048;
      char buffer[SUBJECT_ITEM_MAX_LENGTH + 1] = {0};

      X509_NAME* subj = X509_get_subject_name(cert_);
      if (subj == nullptr) {
        throw std::runtime_error("Failed to retrieve a subject from the certificate");
      }

      if (X509_NAME_get_text_by_NID(subj, itemID, buffer, SUBJECT_ITEM_MAX_LENGTH) == -1) {
        throw std::runtime_error("Failed to retrieve an item from from the certificate subject");
      }

      return buffer;
    }

   private:
    FILE* fp_;
    X509* cert_;
  };

 protected:
  TemporaryDirectory tmp_dir_;
  TestArgs test_args_{tmp_dir_};
  DeviceCredGenerator device_cred_gen_{CERT_PROVIDER_PATH.string()};
};

/**
 *  Verifies generation and serialization of a device private key and a certificate (including its signing)
 *  in case of the fleet credentials usage (i.e. a fleet private key) for the certificate signing.
 *
 *  - [x] Use fleet credentials if provided
 *  - [x] Read fleet CA certificate
 *  - [x] Read fleet private key
 *  - [x] Create device certificate
 *  - [x] Create device keys
 *  - [x] Set public key for the device certificate
 *  - [x] Sign device certificate with fleet private key
 *  - [x] Serialize device private key to a string (we actually can verify only 'searilized' version of the key )
 *  - [x] Serialize device certificate to a string (we actually can verify only 'serialized' version of the certificate)
 *  - [x] Write credentials to a local directory if requested
 *      - [x] Provide device private key
 *      - [x] Provide device certificate
 */

TEST_F(AktualizrCertProviderTest, DeviceCredCreationWithFleetCred) {
  DeviceCredGenerator::ArgSet args;

  args.fleetCA = test_args_.fleet_ca_cert;
  args.fleetCAKey = test_args_.fleet_ca_private_key;
  args.localDir = test_args_.test_dir;

  device_cred_gen_.run(args);
  ASSERT_EQ(device_cred_gen_.lastExitCode(), 0) << device_cred_gen_.lastStdErr();

  DeviceCredGenerator::OutputPath device_cred_path(test_args_.test_dir);

  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.privateKeyFileFullPath))
      << device_cred_path.privateKeyFileFullPath;
  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.certFileFullPath)) << device_cred_path.certFileFullPath;

  Process openssl("/usr/bin/openssl");

  openssl.run({"rsa", "-in", device_cred_path.privateKeyFileFullPath.string(), "-noout", "-check"});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_EQ(openssl.lastStdOut(), "RSA key ok\n") << openssl.lastStdOut();

  openssl.run({"x509", "-in", device_cred_path.certFileFullPath.string(), "-noout", "-pubkey"});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_NE(openssl.lastStdOut().find("-----BEGIN PUBLIC KEY-----\n"), std::string::npos) << openssl.lastStdOut();

  openssl.run({"rsa", "-in", device_cred_path.privateKeyFileFullPath.string(), "-noout", "-modulus"});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  const std::string private_key_modulus = openssl.lastStdOut();

  openssl.run({"x509", "-in", device_cred_path.certFileFullPath.string(), "-noout", "-modulus"});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  const std::string public_key_modulus = openssl.lastStdOut();

  ASSERT_EQ(private_key_modulus, public_key_modulus);

  openssl.run({"verify", "-verbose", "-CAfile", test_args_.fleet_ca_cert, device_cred_path.certFileFullPath.string()});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_EQ(openssl.lastStdOut(), str(boost::format("%1%: OK\n") % device_cred_path.certFileFullPath.string()));
}

/**
 * Verifies cert_provider's output if an incomplete set of fleet credentials is specified.
 * Just  a fleet CA without a fleet private key.
 * Just a fleet private key without a fleet CA
 * Neither `--target` nor `--local` is specified
 *
 * Checks actions:
 *
 *  - [x] Abort if fleet CA is provided without fleet private key
 *  - [x] Abort if fleet private key is provided without fleet CA
 */

TEST_F(AktualizrCertProviderTest, IncompleteFleetCredentials) {
  const std::string expected_error_msg = "fleet-ca and fleet-ca-key options should be used together\n";

  {
    DeviceCredGenerator::ArgSet args;

    args.fleetCA = test_args_.fleet_ca_cert;
    // args.fleetCAKey = test_args_.fleet_ca_private_key;
    args.localDir = test_args_.test_dir;

    device_cred_gen_.run(args);

    EXPECT_EQ(device_cred_gen_.lastExitCode(), 1) << device_cred_gen_.lastStdOut();
    ASSERT_EQ(device_cred_gen_.lastStdErr(), expected_error_msg) << device_cred_gen_.lastStdErr();
  }

  {
    DeviceCredGenerator::ArgSet args;

    // args.fleetCA = test_args_.fleet_ca_cert;
    args.fleetCAKey = test_args_.fleet_ca_private_key;
    args.localDir = test_args_.test_dir;

    device_cred_gen_.run(args);

    EXPECT_EQ(device_cred_gen_.lastExitCode(), 1) << device_cred_gen_.lastStdOut();
    ASSERT_EQ(device_cred_gen_.lastStdErr(), expected_error_msg) << device_cred_gen_.lastStdErr();
  }

  {
    DeviceCredGenerator::ArgSet args;

    args.fleetCA = test_args_.fleet_ca_cert;
    args.fleetCAKey = test_args_.fleet_ca_private_key;
    // args.localDir = test_args_.test_dir;

    device_cred_gen_.run(args);

    ASSERT_EQ(device_cred_gen_.lastExitCode(), 1);
    ASSERT_NE(device_cred_gen_.lastStdErr().find(
                  "Please provide a local directory and/or target to output the generated files to"),
              std::string::npos)
        << device_cred_gen_.lastStdErr();
  }
}

/**
 * Verifies usage of the paths from a config file which is specified via `--config` param.
 * The resultant files's path, private key and certificate files, must correspond to what is specified in the config.
 *
 * Verifies cert_provider's output if both `--directory` and `--config` params are specified
 *
 * Checks actions:
 *
 *  - [x] Use file paths from config if provided
 */
TEST_F(AktualizrCertProviderTest, ConfigFilePathUsage) {
  const std::string base_path = "my_device_cred";
  const std::string private_key_file = "my_device_private_key.pem";
  const std::string cert_file = "my_device_cert.pem";

  Config config;
  config.import.base_path = base_path;
  config.import.tls_pkey_path = BasedPath(private_key_file);
  config.import.tls_clientcert_path = BasedPath(cert_file);

  auto test_conf_file = tmp_dir_ / "conf.toml";
  boost::filesystem::ofstream conf_file(test_conf_file);
  config.writeToStream(conf_file);
  conf_file.close();

  DeviceCredGenerator::OutputPath device_cred_path(test_args_.test_dir, base_path, private_key_file, cert_file);
  DeviceCredGenerator::ArgSet args;

  args.fleetCA = test_args_.fleet_ca_cert;
  args.fleetCAKey = test_args_.fleet_ca_private_key;
  args.localDir = test_args_.test_dir;
  args.configFile = test_conf_file.string();

  device_cred_gen_.run(args);

  ASSERT_EQ(device_cred_gen_.lastExitCode(), 0) << device_cred_gen_.lastStdErr();

  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.privateKeyFileFullPath))
      << "Private key file is missing: " << device_cred_path.privateKeyFileFullPath;
  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.certFileFullPath))
      << "Certificate file is missing: " << device_cred_path.certFileFullPath;

  {
    // The case when both 'directory' and 'config' parameters are specified
    args.directoryPrefix = "whatever-dir";

    device_cred_gen_.run(args);
    EXPECT_EQ(device_cred_gen_.lastExitCode(), 1) << device_cred_gen_.lastStdErr();
    EXPECT_EQ(device_cred_gen_.lastStdErr(),
              "Directory (--directory) and config (--config) options cannot be used together\n")
        << device_cred_gen_.lastStdErr();
  }
}

/**
 * Verifies application of the certificate's and key's parameters specified via parameters
 *
 * Checks actions:
 *
 * - [x] Specify device certificate expiration date
 * - [x] Specify device certificate country code
 * - [x] Specify device certificate state abbreviation
 * - [x] Specify device certificate organization name
 * - [x] Specify device certificate common name
 * - [x] Specify RSA bit length
 */

TEST_F(AktualizrCertProviderTest, DeviceCertParams) {
  const std::string validity_days = "100";
  auto expires_after_sec = (std::stoul(validity_days) * 24 * 3600) + 1;
  std::unordered_map<int, std::string> subject_items = {
      {NID_countryName, "UA"},
      {NID_stateOrProvinceName, "Lviv"},
      {NID_organizationName, "ATS"},
      {NID_commonName, "ats.io"},
  };
  const std::string rsa_bits = "1024";

  DeviceCredGenerator::ArgSet args;

  args.fleetCA = test_args_.fleet_ca_cert;
  args.fleetCAKey = test_args_.fleet_ca_private_key;
  args.localDir = test_args_.test_dir;

  args.validityDays = validity_days;
  args.countryCode = subject_items[NID_countryName];
  args.state = subject_items[NID_stateOrProvinceName];
  args.organization = subject_items[NID_organizationName];
  args.commonName = subject_items[NID_commonName];
  args.rsaBits = rsa_bits;

  device_cred_gen_.run(args);
  ASSERT_EQ(device_cred_gen_.lastExitCode(), 0) << device_cred_gen_.lastStdErr();

  DeviceCredGenerator::OutputPath device_cred_path(test_args_.test_dir);

  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.privateKeyFileFullPath))
      << device_cred_path.privateKeyFileFullPath;
  ASSERT_TRUE(boost::filesystem::exists(device_cred_path.certFileFullPath)) << device_cred_path.certFileFullPath;

  // check subject's params
  Cert cert(device_cred_path.certFileFullPath.string());
  for (auto subject_item : subject_items) {
    ASSERT_EQ(cert.getSubjectItemValue(subject_item.first), subject_item.second);
  }

  Process openssl("/usr/bin/openssl");
  // check RSA length
  const std::string expected_key_str = str(boost::format("Private-Key: (%1% bit") % rsa_bits);
  openssl.run({"rsa", "-in", device_cred_path.privateKeyFileFullPath.string(), "-text", "-noout"});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_NE(openssl.lastStdOut().find(expected_key_str), std::string::npos);

  // check expiration date
  openssl.run({"x509", "-in", device_cred_path.certFileFullPath.string(), "-checkend",
               std::to_string(expires_after_sec - 1024)});

  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdOut();
  ASSERT_NE(openssl.lastStdOut().find("Certificate will not expire"), std::string::npos);

  openssl.run(
      {"x509", "-in", device_cred_path.certFileFullPath.string(), "-checkend", std::to_string(expires_after_sec)});
  ASSERT_EQ(openssl.lastExitCode(), 1) << openssl.lastStdOut();
  ASSERT_NE(openssl.lastStdOut().find("Certificate will expire"), std::string::npos);

  // check signature
  openssl.run({"verify", "-verbose", "-CAfile", test_args_.fleet_ca_cert, device_cred_path.certFileFullPath.string()});
  ASSERT_EQ(openssl.lastExitCode(), 0) << openssl.lastStdErr();
  ASSERT_EQ(openssl.lastStdOut(), str(boost::format("%1%: OK\n") % device_cred_path.certFileFullPath.string()));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc < 2) {
    std::cerr << "A path to the cert_provider is not specified." << std::endl;
    return EXIT_FAILURE;
  }

  CERT_PROVIDER_PATH = argv[1];
  std::cout << "Path to the cert_provider executable: " << CERT_PROVIDER_PATH << std::endl;

  int test_run_res = RUN_ALL_TESTS();

  return test_run_res;
}
#endif
