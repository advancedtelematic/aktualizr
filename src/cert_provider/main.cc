#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "json/json.h"

#include "bootstrap/bootstrap.h"
#include "crypto/crypto.h"
#include "http/httpclient.h"
#include "libaktualizr/config.h"
#include "logging/logging.h"
#include "utilities/aktualizr_version.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

void checkInfoOptions(const bpo::options_description& description, const bpo::variables_map& vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-cert-provider version is: " << aktualizr_version() << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parseOptions(int argc, char** argv) {
  bpo::options_description description("aktualizr-cert-provider command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr-cert-provider version")
      ("credentials,c", bpo::value<boost::filesystem::path>(), "zipped credentials file")
      ("fleet-ca", bpo::value<boost::filesystem::path>(), "path to fleet certificate authority certificate (for signing device certificates)")
      ("fleet-ca-key", bpo::value<boost::filesystem::path>(), "path to the private key of fleet certificate authority")
      ("bits", bpo::value<int>(), "size of RSA keys in bits")
      ("days", bpo::value<int>(), "validity term for the certificate in days")
      ("certificate-c", bpo::value<std::string>(), "value for C field in certificate subject name")
      ("certificate-st", bpo::value<std::string>(), "value for ST field in certificate subject name")
      ("certificate-o", bpo::value<std::string>(), "value for O field in certificate subject name")
      ("certificate-cn", bpo::value<std::string>(), "value for CN field in certificate subject name (used for device ID)")
      ("target,t", bpo::value<std::string>(), "target device to scp credentials to (or [user@]host)")
      ("port,p", bpo::value<int>(), "target port")
      ("directory,d", bpo::value<boost::filesystem::path>(), "directory on target to write credentials to (conflicts with --config)")
      ("root-ca,r", "provide root CA certificate")
      ("server-url,u", "provide server URL file")
      ("local,l", bpo::value<boost::filesystem::path>(), "local directory to write credentials to")
      ("config,g", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory from which to get file names")
      ("skip-checks,s", "skip strict host key checking for ssh/scp commands");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    checkInfoOptions(description, vm);
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
    checkInfoOptions(description, vm);

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

class SSHRunner {
 public:
  SSHRunner(std::string target, const bool skip_checks, const int port = 22)
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

void copyLocal(const boost::filesystem::path& src, const boost::filesystem::path& dest) {
  boost::filesystem::path dest_dir = dest.parent_path();
  if (boost::filesystem::exists(dest_dir)) {
    boost::filesystem::remove(dest);
  } else {
    boost::filesystem::create_directories(dest_dir);
  }
  boost::filesystem::copy_file(src, dest);
}

int main(int argc, char* argv[]) {
  int exit_code = EXIT_FAILURE;

  logger_init();
  logger_set_threshold(static_cast<boost::log::trivial::severity_level>(2));

  try {
    bpo::variables_map commandline_map = parseOptions(argc, argv);

    std::string target;
    if (commandline_map.count("target") != 0) {
      target = commandline_map["target"].as<std::string>();
    }
    int port = 22;
    if (commandline_map.count("port") != 0) {
      port = (commandline_map["port"].as<int>());
    }
    const bool provide_ca = commandline_map.count("root-ca") != 0;
    const bool provide_url = commandline_map.count("server-url") != 0;
    boost::filesystem::path local_dir;
    if (commandline_map.count("local") != 0) {
      local_dir = commandline_map["local"].as<boost::filesystem::path>();
    }
    std::vector<boost::filesystem::path> config_path;
    if (commandline_map.count("config") != 0) {
      config_path = commandline_map["config"].as<std::vector<boost::filesystem::path>>();
    }
    const bool skip_checks = commandline_map.count("skip-checks") != 0;

    boost::filesystem::path fleet_ca_path = "";
    if (commandline_map.count("fleet-ca") != 0) {
      fleet_ca_path = commandline_map["fleet-ca"].as<boost::filesystem::path>();
    }

    boost::filesystem::path fleet_ca_key_path = "";
    if (commandline_map.count("fleet-ca-key") != 0) {
      fleet_ca_key_path = commandline_map["fleet-ca-key"].as<boost::filesystem::path>();
    }

    if (fleet_ca_path.empty() != fleet_ca_key_path.empty()) {
      std::cerr << "fleet-ca and fleet-ca-key options should be used together" << std::endl;
      return EXIT_FAILURE;
    }

    if (!commandline_map["directory"].empty() && !commandline_map["config"].empty()) {
      std::cerr << "Directory (--directory) and config (--config) options cannot be used together" << std::endl;
      return EXIT_FAILURE;
    }

    boost::filesystem::path credentials_path = "";
    if (commandline_map.count("credentials") != 0) {
      credentials_path = commandline_map["credentials"].as<boost::filesystem::path>();
    }

    if (local_dir.empty() && target.empty()) {
      std::cerr << "Please provide a local directory and/or target to output the generated files to" << std::endl;
      return EXIT_FAILURE;
    }

    std::string serverUrl;
    if ((fleet_ca_path.empty() || provide_ca || provide_url) && credentials_path.empty()) {
      std::cerr
          << "Error: missing -c/--credentials parameters which is mandatory if the fleet CA is not specified or an "
             "output of the root CA or a gateway URL is requested";
      return EXIT_FAILURE;
    } else {
      serverUrl = Bootstrap::readServerUrl(credentials_path);
    }

    std::string device_id;
    if (commandline_map.count("certificate-cn") != 0) {
      device_id = (commandline_map["certificate-cn"].as<std::string>());
      if (device_id.empty()) {
        std::cerr << "Common name (device ID, --certificate-cn) can't be empty" << std::endl;
        return EXIT_FAILURE;
      }
    } else {
      device_id = Utils::genPrettyName();
      std::cout << "Random device ID is " << device_id << "\n";
    }

    boost::filesystem::path directory = "/var/sota/import";
    utils::BasedPath pkey_file = utils::BasedPath("pkey.pem");
    utils::BasedPath cert_file = utils::BasedPath("client.pem");
    utils::BasedPath ca_file = utils::BasedPath("root.crt");
    utils::BasedPath url_file = utils::BasedPath("gateway.url");
    if (!config_path.empty()) {
      Config config(config_path);

      // try first import base path and then storage path
      if (!config.import.base_path.empty()) {
        directory = config.import.base_path;
      } else if (!config.storage.path.empty()) {
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
      if (provide_url && !config.tls.server_url_path.empty()) {
        url_file = config.tls.server_url_path;
      }
    }

    if (!commandline_map["directory"].empty()) {
      directory = commandline_map["directory"].as<boost::filesystem::path>();
    }

    TemporaryFile tmp_pkey_file(pkey_file.get("").filename().string());
    TemporaryFile tmp_cert_file(cert_file.get("").filename().string());
    TemporaryFile tmp_ca_file(ca_file.get("").filename().string());
    TemporaryFile tmp_url_file(url_file.get("").filename().string());

    std::string pkey;
    std::string cert;
    std::string ca;

    if (fleet_ca_path.empty()) {  // no fleet CA => provision with shared credentials
      Bootstrap boot(credentials_path, "");
      HttpClient http;
      Json::Value data;
      data["deviceId"] = device_id;
      data["ttl"] = 36000;

      std::cout << "Provisioning against server...\n";
      http.setCerts(boot.getCa(), CryptoSource::kFile, boot.getCert(), CryptoSource::kFile, boot.getPkey(),
                    CryptoSource::kFile);
      HttpResponse response = http.post(serverUrl + "/devices", data);
      if (!response.isOk()) {
        Json::Value resp_code = response.getJson()["code"];
        if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
          std::cout << "Device ID" << device_id << "is occupied.\n";
          return EXIT_FAILURE;
        }
        std::cout << "Provisioning failed, response: " << response.body << "\n";
        return EXIT_FAILURE;
      }
      std::cout << "...success\n";

      StructGuard<BIO> device_p12(BIO_new_mem_buf(response.body.c_str(), static_cast<int>(response.body.size())),
                                  BIO_vfree);
      if (!Crypto::parseP12(device_p12.get(), "", &pkey, &cert, &ca)) {
        std::cout << "Unable to parse p12 file received from server.\n";
        return EXIT_FAILURE;
      }
    } else {  // fleet CA set => generate and sign a new certificate
      int rsa_bits = 2048;
      if (commandline_map.count("bits") != 0) {
        rsa_bits = (commandline_map["bits"].as<int>());
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
          return EXIT_FAILURE;
        }
      }

      std::string newcert_st;
      if (commandline_map.count("certificate-st") != 0) {
        newcert_st = (commandline_map["certificate-st"].as<std::string>());
        if (newcert_st.empty()) {
          std::cerr << "State name (--certificate-st) can't be empty" << std::endl;
          return EXIT_FAILURE;
        }
      }

      std::string newcert_o;
      if (commandline_map.count("certificate-o") != 0) {
        newcert_o = (commandline_map["certificate-o"].as<std::string>());
        if (newcert_o.empty()) {
          std::cerr << "Organization name (--certificate-o) can't be empty" << std::endl;
          return EXIT_FAILURE;
        }
      }

      StructGuard<X509> certificate =
          Crypto::generateCert(rsa_bits, cert_days, newcert_c, newcert_st, newcert_o, device_id);
      Crypto::signCert(fleet_ca_path.native(), fleet_ca_key_path.native(), certificate.get());
      Crypto::serializeCert(&pkey, &cert, certificate.get());

      if (provide_ca) {
        // Read server root CA from server_ca.pem in archive if found (to support
        // community edition use case). Otherwise, default to the old version of
        // expecting it to be in the p12.
        ca = Bootstrap::readServerCa(credentials_path);
        if (ca.empty()) {
          Bootstrap boot(credentials_path, "");
          ca = boot.getCa();
          std::cout << "Server root CA read from autoprov_credentials.p12 in zipped archive.\n";
        } else {
          std::cout << "Server root CA read from server_ca.pem in zipped archive.\n";
        }
      }
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
      auto pkey_file_path = local_dir / pkey_file.get(directory);
      std::cout << "Writing the generated client private key to " << pkey_file_path << " ...\n";
      copyLocal(tmp_pkey_file.PathString(), pkey_file_path);

      auto cert_file_path = local_dir / cert_file.get(directory);
      std::cout << "Writing the generated and signed client certificate to " << cert_file_path << " ...\n";
      copyLocal(tmp_cert_file.PathString(), cert_file_path);

      if (provide_ca) {
        auto root_ca_file = local_dir / ca_file.get(directory);
        std::cout << "Writing the server root CA to " << root_ca_file << " ...\n";
        copyLocal(tmp_ca_file.PathString(), root_ca_file);
      }
      if (provide_url) {
        auto gtw_url_file = local_dir / url_file.get(directory);
        std::cout << "Writing the gateway URL to " << gtw_url_file << " ...\n";
        copyLocal(tmp_url_file.PathString(), gtw_url_file);
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
        ssh.transferFile(tmp_pkey_file.Path(), pkey_file.get(directory));

        ssh.transferFile(tmp_cert_file.Path(), cert_file.get(directory));

        if (provide_ca) {
          ssh.transferFile(tmp_ca_file.Path(), ca_file.get(directory));
        }
        if (provide_url) {
          ssh.transferFile(tmp_url_file.Path(), url_file.get(directory));
        }

        std::cout << "...success\n";
      } catch (const std::runtime_error& exc) {
        std::cout << exc.what() << std::endl;
      }
    }

    exit_code = EXIT_SUCCESS;
  } catch (const std::exception& exc) {
    LOG_ERROR << "Error: " << exc.what();

    exit_code = EXIT_FAILURE;
  }

  return exit_code;
}
