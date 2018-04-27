#ifndef HTTPFAKE_H_
#define HTTPFAKE_H_

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fstream>

#include "json/json.h"

#include "crypto/crypto.h"
#include "http/httpinterface.h"
#include "utilities/utils.h"

enum ProvisioningResult { ProvisionOK, ProvisionFailure };

class HttpFake : public HttpInterface {
 public:
  HttpFake(const boost::filesystem::path &test_dir_in, const bool is_initialized = false)
      : provisioningResponse(ProvisionOK), test_dir(test_dir_in), manifest_count(0), ecu_registered(is_initialized) {
    boost::filesystem::copy_directory("tests/test_data/repo", metadata_path.Path() / "repo");
    boost::filesystem::copy_directory("tests/test_data/director", metadata_path.Path() / "director");
  }

  ~HttpFake() {}

  virtual void setCerts(const std::string &ca, CryptoSource ca_source, const std::string &cert,
                        CryptoSource cert_source, const std::string &pkey, CryptoSource pkey_source) {
    (void)ca;
    (void)ca_source;
    (void)cert;
    (void)cert_source;
    (void)pkey;
    (void)pkey_source;
  }

  HttpResponse get(const std::string &url) {
    std::cout << "URL:" << url << "\n";
    if (url.find(tls_server) == 0) {
      boost::filesystem::path path = metadata_path.Path() / url.substr(tls_server.size());
      std::cout << "filetoopen: " << path << "\n\n\n";
      if (url.find("timestamp.json") != std::string::npos) {
        std::cout << "CHECK PATH: " << (metadata_path.Path() / "timestamp.json").string() << "\n";
        if (boost::filesystem::exists(path)) {
          boost::filesystem::copy_file("tests/test_data/timestamp2.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        } else {
          boost::filesystem::copy_file("tests/test_data/timestamp1.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        }
        return HttpResponse(Utils::readFile(path), 200, CURLE_OK, "");
      } else if (url.find("targets.json") != std::string::npos) {
        Json::Value timestamp = Utils::parseJSONFile(metadata_path.Path() / "repo/timestamp.json");
        if (timestamp["signed"]["version"].asInt64() == 2) {
          return HttpResponse(Utils::readFile("tests/test_data/targets_noupdates.json"), 200, CURLE_OK, "");
        } else {
          return HttpResponse(Utils::readFile("tests/test_data/targets_hasupdates.json"), 200, CURLE_OK, "");
        }
      } else {
        return HttpResponse(Utils::readFile("tests/test_data/" + url.substr(tls_server.size())), 200, CURLE_OK, "");
      }
    }
    return HttpResponse(url, 200, CURLE_OK, "");
  }

  HttpResponse post(const std::string &url, const Json::Value &data) {
    if (url.find("tst149") == 0) {
      if (url == "tst149/devices") {
        EXPECT_EQ(data["deviceId"].asString(), "tst149_device_id");
        return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
      } else if (url == "tst149/director/ecus") {
        EXPECT_EQ(data["primary_ecu_serial"].asString(), "tst149_ecu_serial");
        EXPECT_EQ(data["ecus"][0]["hardware_identifier"].asString(), "tst149_hardware_identifier");
        EXPECT_EQ(data["ecus"][0]["ecu_serial"].asString(), "tst149_ecu_serial");
        if (!ecu_registered) {
          ecu_registered = true;
          return HttpResponse("{}", 200, CURLE_OK, "");
        } else {
          return HttpResponse("{\"code\":\"ecu_already_registered\"}", 409, CURLE_OK, "");
        }
      }
    }
    Utils::writeFile((test_dir / "post.json").string(), data);
    if (provisioningResponse == ProvisionOK) {
      return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
    } else {
      return HttpResponse("", 400, CURLE_OK, "");
    }
  }

  HttpResponse put(const std::string &url, const Json::Value &data) {
    if (url == "tst149/core/installed") {
      EXPECT_EQ(data.size(), 1);
      EXPECT_EQ(data[0]["name"].asString(), "fake-package");
      EXPECT_EQ(data[0]["version"].asString(), "1.0");
    } else if (url == "tst149/core/system_info") {
      Json::Value hwinfo = Utils::getHardwareInfo();
      EXPECT_EQ(hwinfo["id"].asString(), data["id"].asString());
      EXPECT_EQ(hwinfo["description"].asString(), data["description"].asString());
      EXPECT_EQ(hwinfo["class"].asString(), data["class"].asString());
      EXPECT_EQ(hwinfo["product"].asString(), data["product"].asString());
    } else if (url == "tst149/director/manifest") {
      std::string hash;
      if (manifest_count == 0) {
        // Check for default initial value of packagemanagerfake.
        hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
        ++manifest_count;
      } else {
        hash = "tst149_ecu_serial";
      }
      EXPECT_EQ(data["signed"]["ecu_version_manifests"]["tst149_ecu_serial"]["signed"]["installed_image"]["filepath"]
                    .asString(),
                (hash == "tst149_ecu_serial") ? hash : "unknown");
      EXPECT_EQ(data["signed"]["ecu_version_manifests"]["tst149_ecu_serial"]["signed"]["installed_image"]["fileinfo"]
                    ["hashes"]["sha256"]
                        .asString(),
                hash);
    }

    std::ofstream director_file((test_dir / test_manifest).c_str());
    director_file << data;
    director_file.close();
    return HttpResponse(url, 200, CURLE_OK, "");
  }

  HttpResponse download(const std::string &url, curl_write_callback callback, void *userp) {
    (void)callback;
    (void)userp;
    std::cout << "URL: " << url << "\n";
    boost::filesystem::path path = test_dir / url.substr(url.rfind("/targets/") + 9);
    std::cout << "filetoopen: " << path << "\n\n\n";

    std::string content = Utils::readFile(path.string());

    // Hack since the signature strangely requires non-const.
    callback(const_cast<char *>(content.c_str()), content.size(), 1, userp);
    return HttpResponse(content, 200, CURLE_OK, "");
  }

  ProvisioningResult provisioningResponse;
  const std::string test_manifest = "test_aktualizr_manifest.txt";
  const std::string tls_server = "https://tlsserver.com";

 private:
  /**
   * These are here to catch a common programming error where a Json::Value is
   * implicitly constructed from a std::string. By having an private overload
   * that takes string (and with no implementation), this will fail during
   * compilation.
   */
  HttpResponse post(const std::string &url, const std::string data);
  HttpResponse put(const std::string &url, const std::string data);

  boost::filesystem::path test_dir;
  TemporaryDirectory metadata_path;
  int manifest_count;
  bool ecu_registered;
};

#endif  // HTTPFAKE_H_
