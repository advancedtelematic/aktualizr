#include <stdlib.h>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <utility>

#include <openssl/ssl.h>
#include "json/json.h"

#include "bootstrap.h"
#include "logging/logging.h"
#include "utilities/crypto.h"
#include "utilities/httpclient.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description& description, const bpo::variables_map& vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr_cert_provider version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char* argv[]) {
  bpo::options_description description("aktualizr_cert_provider command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr_cert_provider version")
      ("credentials,c", bpo::value<boost::filesystem::path>()->required(), "zipped credentials file")
      ("device-ca", bpo::value<boost::filesystem::path>(), "path to certificate authority certificate signing device certificates")
      ("device-ca-key", bpo::value<boost::filesystem::path>(), "path to the private key of device certificate authority")
      ("bits", bpo::value<int>(), "size of RSA keys in bits")
      ("days", bpo::value<int>(), "validity term for the certificate in days")
      ("certificate-c", bpo::value<std::string>(), "value for C field in certificate subject name")
      ("certificate-st", bpo::value<std::string>(), "value for ST field in certificate subject name")
      ("certificate-o", bpo::value<std::string>(), "value for O field in certificate subject name")
      ("certificate-cn", bpo::value<std::string>(), "value for CN field in certificate subject name")
      ("target,t", bpo::value<std::string>(), "target device to scp credentials to (or [user@]host)")
      ("port,p", bpo::value<int>(), "target port")
      ("directory,d", bpo::value<boost::filesystem::path>(), "directory on target to write credentials to (conflicts with -config)")
      ("root-ca,r", "provide root CA")
      ("server-url,u", "provide server url file")
      ("local,l", bpo::value<boost::filesystem::path>(), "local directory to write credentials to")
      ("config,g", bpo::value<boost::filesystem::path>(), "sota.toml configuration file from which to get file names")
      ("skip-checks,s", "skip strict host key checking for ssh/scp commands");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
    unregistered_options = bpo::collect_unrecognized(parsed_options.options, bpo::include_positional);
    if (vm.count("help") == 0 && !unregistered_options.empty()) {
      std::cout << description << "\n";
      exit(EXIT_FAILURE);
    }
  } catch (const bpo::required_option& ex) {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_FAILURE);
  } catch (const bpo::error& ex) {
    check_info_options(description, vm);

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

// I miss Rust's ? operator
#define SSL_ERROR(description)                                                           \
  {                                                                                      \
    std::cerr << description << ERR_error_string(ERR_get_error(), nullptr) << std::endl; \
    return false;                                                                        \
  }
bool generate_and_sign(const std::string& cacert_path, const std::string& capkey_path, std::string* pkey,
                       std::string* cert, const bpo::variables_map& commandline_map) {
  int rsa_bits = 2048;
  if (commandline_map.count("bits") != 0) {
    rsa_bits = (commandline_map["bits"].as<int>());
    if (rsa_bits < 31) {  // sic!
      std::cerr << "RSA key size can't be smaller than 31 bits" << std::endl;
      return false;
    }
  }

  int cert_days = 365;
  if (commandline_map.count("days") != 0) {
    cert_days = (commandline_map["days"].as<int>());
  }

  std::string newcert_c;
  if (commandline_map.count("certificate-c") != 0) {
    newcert_c = (commandline_map["certificate-c"].as<std::string>());
    if (newcert_c.length() != 2) {
      std::cerr << "Country code (--certificate-c) should be 2 characters long" << std::endl;
      return false;
    }
  };

  std::string newcert_st;
  if (commandline_map.count("certificate-st") != 0) {
    newcert_st = (commandline_map["certificate-st"].as<std::string>());
    if (newcert_st.empty()) {
      std::cerr << "State name (--certificate-st) can't be empty" << std::endl;
      return false;
    }
  };

  std::string newcert_o;
  if (commandline_map.count("certificate-o") != 0) {
    newcert_o = (commandline_map["certificate-o"].as<std::string>());
    if (newcert_o.empty()) {
      std::cerr << "Organization name (--certificate-o) can't be empty" << std::endl;
      return false;
    }
  };

  std::string newcert_cn;
  if (commandline_map.count("certificate-cn") != 0) {
    newcert_cn = (commandline_map["certificate-cn"].as<std::string>());
    if (newcert_cn.empty()) {
      std::cerr << "Common name (--certificate-cn) can't be empty" << std::endl;
      return false;
    }
  } else {
    newcert_cn = Utils::genPrettyName();
  }

  // read CA certificate
  std::string cacert_contents = Utils::readFile(cacert_path);
  StructGuard<BIO> bio_in_cacert(BIO_new_mem_buf(cacert_contents.c_str(), static_cast<int>(cacert_contents.size())),
                                 BIO_free_all);
  StructGuard<X509> ca_certificate(PEM_read_bio_X509(bio_in_cacert.get(), nullptr, nullptr, nullptr), X509_free);
  if (ca_certificate.get() == nullptr) {
    std::cerr << "Reading CA certificate failed"
              << "\n";
    return false;
  }

  // read CA private key
  std::string capkey_contents = Utils::readFile(capkey_path);
  StructGuard<BIO> bio_in_capkey(BIO_new_mem_buf(capkey_contents.c_str(), static_cast<int>(capkey_contents.size())),
                                 BIO_free_all);
  StructGuard<EVP_PKEY> ca_privkey(PEM_read_bio_PrivateKey(bio_in_capkey.get(), nullptr, nullptr, nullptr),
                                   EVP_PKEY_free);
  if (ca_privkey.get() == nullptr) SSL_ERROR("PEM_read_bio_PrivateKey failed: ");

  // create certificate
  StructGuard<X509> certificate(X509_new(), X509_free);
  if (certificate.get() == nullptr) SSL_ERROR("X509_new failed: ");

  X509_set_version(certificate.get(), 2);  // X509v3

  {
    std::random_device urandom;
    std::uniform_int_distribution<> serial_dist(0, (1UL << 20) - 1);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), serial_dist(urandom));
  }

  //   create and set certificate subject name
  StructGuard<X509_NAME> subj(X509_NAME_new(), X509_NAME_free);
  if (subj.get() == nullptr) SSL_ERROR("X509_NAME_new failed: ");

  if (!newcert_c.empty()) {
    if (X509_NAME_add_entry_by_txt(subj.get(), "C", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(newcert_c.c_str()), -1, -1, 0) == 0)
      SSL_ERROR("X509_NAME_add_entry_by_txt failed: ");
  }

  if (!newcert_st.empty()) {
    if (X509_NAME_add_entry_by_txt(subj.get(), "ST", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(newcert_st.c_str()), -1, -1, 0) == 0)
      SSL_ERROR("X509_NAME_add_entry_by_txt failed: ");
  }

  if (!newcert_o.empty()) {
    if (X509_NAME_add_entry_by_txt(subj.get(), "O", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(newcert_o.c_str()), -1, -1, 0) == 0)
      SSL_ERROR("X509_NAME_add_entry_by_txt failed: ");
  }

  assert(!newcert_cn.empty());
  if (X509_NAME_add_entry_by_txt(subj.get(), "CN", MBSTRING_ASC,
                                 reinterpret_cast<const unsigned char*>(newcert_cn.c_str()), -1, -1, 0) == 0)
    SSL_ERROR("X509_NAME_add_entry_by_txt failed: ");

  if (X509_set_subject_name(certificate.get(), subj.get()) == 0) SSL_ERROR("X509_set_subject_name failed: ");

  //    set issuer name
  X509_NAME* ca_subj = X509_get_subject_name(ca_certificate.get());
  if (ca_subj == nullptr) SSL_ERROR("X509_get_subject_name failed: ");

  if (X509_set_issuer_name(certificate.get(), ca_subj) == 0) SSL_ERROR("X509_set_issuer_name failed: ");

  // create and set key

  // freed by owner EVP_PKEY
  RSA* certificate_rsa = RSA_generate_key(rsa_bits, RSA_F4, nullptr, nullptr);
  if (certificate_rsa == nullptr) SSL_ERROR("RSA_generate_key failed: ");

  StructGuard<EVP_PKEY> certificate_pkey(EVP_PKEY_new(), EVP_PKEY_free);
  if (certificate_pkey.get() == nullptr) SSL_ERROR("EVP_PKEY_new failed: ");

  if (!EVP_PKEY_assign_RSA(certificate_pkey.get(), certificate_rsa))  // NOLINT
    SSL_ERROR("EVP_PKEY_assign_RSA failed: ");

  if (X509_set_pubkey(certificate.get(), certificate_pkey.get()) == 0) SSL_ERROR("X509_set_pubkey failed: ");

  //    set validity period
  if (X509_gmtime_adj(X509_get_notBefore(certificate.get()), 0) == nullptr) SSL_ERROR("X509_gmtime_adj failed: ");

  if (X509_gmtime_adj(X509_get_notAfter(certificate.get()), 60L * 60L * 24L * cert_days) == nullptr)
    SSL_ERROR("X509_gmtime_adj failed: ");

  //    sign
  const EVP_MD* cert_digest = EVP_sha256();
  if (X509_sign(certificate.get(), ca_privkey.get(), cert_digest) == 0) SSL_ERROR("X509_sign failed: ");

  // serialize private key
  char* privkey_buf;
  StructGuard<BIO> privkey_file(BIO_new(BIO_s_mem()), BIO_vfree);
  if (privkey_file == nullptr) {
    std::cerr << "Error opening memstream" << std::endl;
    return false;
  }
  int ret = PEM_write_bio_RSAPrivateKey(privkey_file.get(), certificate_rsa, nullptr, nullptr, 0, nullptr, nullptr);
  if (ret == 0) {
    std::cerr << "PEM_write_RSAPrivateKey" << std::endl;
    return false;
  }
  auto privkey_len = BIO_get_mem_data(privkey_file.get(), &privkey_buf);  // NOLINT
  *pkey = std::string(privkey_buf, privkey_len);

  // serialize certificate
  char* cert_buf;
  StructGuard<BIO> cert_file(BIO_new(BIO_s_mem()), BIO_vfree);
  if (cert_file == nullptr) {
    std::cerr << "Error opening memstream" << std::endl;
    return false;
  }
  ret = PEM_write_bio_X509(cert_file.get(), certificate.get());
  if (ret == 0) {
    std::cerr << "PEM_write_X509" << std::endl;
    return false;
  }
  auto cert_len = BIO_get_mem_data(cert_file.get(), &cert_buf);  // NOLINT
  *cert = std::string(cert_buf, cert_len);

  return true;
}

class SSHRunner {
 public:
  SSHRunner(std::string target, bool skip_checks, int port = 22)
      : target_(std::move(target)), skip_checks_(skip_checks), port_(port) {}

  void runCmd(const std::string& cmd) const {
    std::ostringstream prefix;

    prefix << "ssh ";
    if (port_ != 22) {
      prefix << "-p " << port_ << " ";
    }
    if (skip_checks_) {
      prefix << "-o StrictHostKeyChecking=no ";
    }
    prefix << target_ << " ";

    std::string fullCmd = prefix.str() + cmd;
    std::cout << "Running " << fullCmd << std::endl;

    int ret = system(fullCmd.c_str());
    if (ret != 0) {
      throw std::runtime_error("Error running command on " + target_ + ": " + std::to_string(ret));
    }
  }

  void transferFile(const boost::filesystem::path& inFile, const boost::filesystem::path& targetPath) {
    // create parent directory
    runCmd("mkdir -p " + targetPath.parent_path().string());

    std::ostringstream prefix;

    prefix << "scp ";
    if (port_ != 22) {
      prefix << "-P " << port_ << " ";
    }
    if (skip_checks_) {
      prefix << "-o StrictHostKeyChecking=no ";
    }

    std::string fullCmd = prefix.str() + inFile.string() + " " + target_ + ":" + targetPath.string();
    std::cout << "Running " << fullCmd << std::endl;

    int ret = system(fullCmd.c_str());
    if (ret != 0) {
      throw std::runtime_error("Error copying file on " + target_ + ": " + std::to_string(ret));
    }
  }

 private:
  std::string target_;
  bool skip_checks_;
  int port_;
};

int main(int argc, char* argv[]) {
  logger_init();
  SSL_load_error_strings();
  logger_set_threshold(static_cast<boost::log::trivial::severity_level>(2));

  bpo::variables_map commandline_map = parse_options(argc, argv);

  boost::filesystem::path credentials_path = commandline_map["credentials"].as<boost::filesystem::path>();
  std::string target;
  if (commandline_map.count("target") != 0) {
    target = commandline_map["target"].as<std::string>();
  }
  int port = 22;
  if (commandline_map.count("port") != 0) {
    port = (commandline_map["port"].as<int>());
  }
  bool provide_ca = commandline_map.count("root-ca") != 0;
  bool provide_url = commandline_map.count("server-url") != 0;
  boost::filesystem::path local_dir;
  if (commandline_map.count("local") != 0) {
    local_dir = commandline_map["local"].as<boost::filesystem::path>();
  }
  boost::filesystem::path config_path = "";
  if (commandline_map.count("config") != 0) {
    config_path = commandline_map["config"].as<boost::filesystem::path>();
  }
  bool skip_checks = commandline_map.count("skip-checks") != 0;

  boost::filesystem::path device_ca_path = "";
  if (commandline_map.count("device-ca") != 0) {
    device_ca_path = commandline_map["device-ca"].as<boost::filesystem::path>();
  }

  boost::filesystem::path device_ca_key_path = "";
  if (commandline_map.count("device-ca-key") != 0) {
    device_ca_key_path = commandline_map["device-ca-key"].as<boost::filesystem::path>();
  }

  if (device_ca_path.empty() != device_ca_key_path.empty()) {
    std::cerr << "device-ca and device-ca-key options should be used together" << std::endl;
    return 1;
  }

  if (!commandline_map["directory"].empty() && !commandline_map["config"].empty()) {
    std::cerr << "Directory (--directory) and config (--config) options cannot be used together" << std::endl;
    return 1;
  }

  boost::filesystem::path directory = "/var/sota/token";
  boost::filesystem::path pkey_file = "pkey.pem";
  boost::filesystem::path cert_file = "client.pem";
  boost::filesystem::path ca_file = "root.crt";
  boost::filesystem::path url_file = "gateway.url";
  if (!config_path.empty()) {
    Config config(config_path);
    // TODO: provide path to root directory in `--local` parameter

    if (!config.storage.path.empty()) {
      directory = config.storage.path;
    }

    if (!config.import.tls_pkey_path.empty()) {
      pkey_file = config.import.tls_pkey_path;
    } else {
      pkey_file = config.storage.tls_pkey_path;
    }

    if (!config.import.tls_clientcert_path.empty()) {
      cert_file = config.import.tls_clientcert_path;
    } else {
      cert_file = config.storage.tls_clientcert_path;
    }
    if (provide_ca) {
      if (!config.import.tls_cacert_path.empty()) {
        ca_file = config.import.tls_cacert_path;
      } else {
        ca_file = config.storage.tls_cacert_path;
      }
    }
    if (provide_url) {
      url_file = config.tls.server_url_path;
    }
  }

  if (!commandline_map["directory"].empty()) {
    directory = commandline_map["directory"].as<boost::filesystem::path>();
  }

  TemporaryFile tmp_pkey_file(pkey_file.filename().string());
  TemporaryFile tmp_cert_file(cert_file.filename().string());
  TemporaryFile tmp_ca_file(ca_file.filename().string());
  TemporaryFile tmp_url_file(url_file.filename().string());

  std::string pkey;
  std::string cert;
  std::string ca;
  std::string serverUrl;

  if (device_ca_path.empty()) {  // no device ca => autoprovision
    std::string device_id = Utils::genPrettyName();
    std::cout << "Random device ID is " << device_id << "\n";

    Bootstrap boot(credentials_path, "");
    HttpClient http;
    Json::Value data;
    data["deviceId"] = device_id;
    data["ttl"] = 36000;
    serverUrl = Bootstrap::readServerUrl(credentials_path);

    std::cout << "Provisioning against server...\n";
    http.setCerts(boot.getCa(), kFile, boot.getCert(), kFile, boot.getPkey(), kFile);
    HttpResponse response = http.post(serverUrl + "/devices", data);
    if (!response.isOk()) {
      Json::Value resp_code = response.getJson()["code"];
      if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
        std::cout << "Device ID" << device_id << "is occupied.\n";
        return -1;
      }
      std::cout << "Provisioning failed, response: " << response.body << "\n";
      return -1;
    }
    std::cout << "...success\n";

    StructGuard<BIO> device_p12(BIO_new_mem_buf(response.body.c_str(), response.body.size()), BIO_vfree);
    if (!Crypto::parseP12(device_p12.get(), "", &pkey, &cert, &ca)) {
      return -1;
    }
  } else {  // device CA set => generate and sign a new certificate
    if (!generate_and_sign(device_ca_path.native(), device_ca_key_path.native(), &pkey, &cert, commandline_map)) {
      return 1;
    }
    // TODO: backend should issue credentials.zip for implicit provision that will contain
    //  root CA certificate as a separate file. Until then extract it from autoprov.p12
    Bootstrap boot(credentials_path, "");
    ca = boot.getCa();
    serverUrl = Bootstrap::readServerUrl(credentials_path);
  }

  tmp_pkey_file.PutContents(pkey);
  tmp_cert_file.PutContents(cert);
  if (provide_ca) {
    tmp_ca_file.PutContents(ca);
  }
  if (provide_url) {
    tmp_url_file.PutContents(serverUrl);
  }

  if (!local_dir.empty()) {
    std::cout << "Writing client certificate and keys to " << local_dir << " ...\n";
    if (boost::filesystem::exists(local_dir)) {
      boost::filesystem::remove(local_dir / pkey_file);
      boost::filesystem::remove(local_dir / cert_file);
      if (provide_ca) {
        boost::filesystem::remove(local_dir / ca_file);
      }
      if (provide_url) {
        boost::filesystem::remove(local_dir / url_file);
      }
    } else {
      boost::filesystem::create_directory(local_dir);
    }
    boost::filesystem::copy_file(tmp_pkey_file.PathString(), local_dir / pkey_file);
    boost::filesystem::copy_file(tmp_cert_file.PathString(), local_dir / cert_file);
    if (provide_ca) {
      boost::filesystem::copy_file(tmp_ca_file.PathString(), local_dir / ca_file);
    }
    if (provide_url) {
      boost::filesystem::copy_file(tmp_url_file.PathString(), local_dir / url_file);
    }
    std::cout << "...success\n";
  }

  if (!target.empty()) {
    std::cout << "Copying client certificate and keys to " << target << ":" << directory;
    if (port != 0) {
      std::cout << " on port " << port;
    }
    std::cout << " ...\n";

    SSHRunner ssh{target, skip_checks, port};

    try {
      ssh.transferFile(tmp_pkey_file.Path(), Utils::absolutePath(directory, pkey_file));

      ssh.transferFile(tmp_cert_file.Path(), Utils::absolutePath(directory, cert_file));

      if (provide_ca) {
        ssh.transferFile(tmp_ca_file.Path(), Utils::absolutePath(directory, ca_file));
      }
      if (provide_url) {
        ssh.transferFile(tmp_url_file.Path(), Utils::absolutePath(directory, url_file));
      }

      std::cout << "...success\n";
    } catch (const std::runtime_error& exc) {
      std::cout << exc.what() << std::endl;
    }
  }

  ERR_free_strings();
  return 0;
}
