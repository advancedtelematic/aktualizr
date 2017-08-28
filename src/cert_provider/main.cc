#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "json/json.h"

#include "bootstrap.h"
#include "crypto.h"
#include "httpclient.h"
#include "logger.h"
#include "utils.h"

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current cert_provider version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("CommandLine Options");
  // clang-format off
  description.add_options()
      ("help,h", "help screen")
      ("version,v", "Current cert_provider version")
      ("credentials,c", bpo::value<std::string>()->required(), "zipped credentials file")
      ("target,t", bpo::value<std::string>(), "target device")
      ("port,p", bpo::value<int>(), "target port");
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
  } catch (const bpo::required_option &ex) {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_SUCCESS);
  } catch (const bpo::error &ex) {
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

int main(int argc, char *argv[]) {
  loggerInit();
  loggerSetSeverity(static_cast<LoggerLevels>(2));

  boost::filesystem::remove("device_id");
  boost::filesystem::remove("root.crt");
  boost::filesystem::remove("client.pem");
  boost::filesystem::remove("pkey.pem");

  bpo::variables_map commandline_map = parse_options(argc, argv);

  std::string credentials_path = commandline_map["credentials"].as<std::string>();
  std::string target = "";
  if (commandline_map.count("target") != 0) {
    target = commandline_map["target"].as<std::string>();
  }
  int port = 0;
  if (commandline_map.count("port") != 0) {
    port = (commandline_map["port"].as<int>());
  }

  std::string device_id = Utils::genPrettyName();
  std::cout << "Random device ID is " << device_id << "\n";

  Bootstrap boot(credentials_path, "");
  HttpClient http;
  Json::Value data;
  data["deviceId"] = device_id;
  data["ttl"] = 36000;
  std::string serverUrl = Bootstrap::readServerUrl(credentials_path);

  std::cout << "Provisioning against server...\n";
  http.setCerts(boot.getCa(), boot.getCert(), boot.getPkey());
  HttpResponse response = http.post(serverUrl + "/devices", data);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
      std::cout << "Device ID" << device_id << "is occupied.\n";
      return -1;
    } else {
      std::cout << "Provisioning failed, response: " << response.body << "\n";
      return -1;
    }
  }
  std::cout << "...success\n";

  std::string pkey;
  std::string cert;
  std::string ca;
  FILE *device_p12 = fmemopen(const_cast<char *>(response.body.c_str()), response.body.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", &pkey, &cert, &ca)) {
    return -1;
  }
  fclose(device_p12);

  Utils::writeFile("device_id", device_id);
  Utils::writeFile("root.crt", ca);
  Utils::writeFile("client.pem", cert);
  Utils::writeFile("pkey.pem", pkey);
  sync();

  if (!target.empty()) {
    std::cout << "Copying client certificate and keys to " << target << ":/var/sota/";
    if (port) {
      std::cout << " on port " << port;
    }
    std::cout << "\n";
    std::ostringstream scp_stream;
    scp_stream << "scp";
    if (port) {
      scp_stream << " -P " << port;
    }
    scp_stream << " device_id root.crt client.pem pkey.pem " << target << ":/var/sota/";
    system(scp_stream.str().c_str());
    std::cout << "...success\n";
  }

  return 0;
}
