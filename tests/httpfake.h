#ifndef HTTPFAKE_H_
#define HTTPFAKE_H_

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include "json/json.h"

#include "crypto/crypto.h"
#include "http/httpinterface.h"
#include "utilities/utils.h"

enum class ProvisioningResult { kOK, kFailure };

class HttpFake : public HttpInterface {
 public:
  HttpFake(const boost::filesystem::path &test_dir_in)
      : provisioningResponse(ProvisioningResult::kOK), test_dir(test_dir_in) {
    Utils::copyDir("tests/test_data/repo/repo/image", metadata_path.Path() / "repo");
    Utils::copyDir("tests/test_data/repo/repo/director", metadata_path.Path() / "director");
    Utils::copyDir("tests/test_data/repo/campaigner", metadata_path.Path() / "campaigner");
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

  /**
   * The noupdates parts are only used by Uptane.FetchNoUpdates.
   * The multisec parts are only used by Uptane.InstallMultipleSecondaries.
   * The unstable parts are only used by Uptane.restoreVerify, although they
   * mostly use the same hasupdates files.
   *
   * All other Uptane tests use the hasupdates set.
   */
  HttpResponse get(const std::string &url, int64_t maxsize) {
    (void)maxsize;

    std::cout << "URL requested: " << url << "\n";
    if (url.find(tls_server) == 0) {
      boost::filesystem::path path;
      if (url.find("unstable/") != std::string::npos) {
        if (unstable_valid_count >= unstable_valid_num) {
          ++unstable_valid_num;
          unstable_valid_count = 0;
          return HttpResponse({}, 503, CURLE_OK, "");
        } else {
          ++unstable_valid_count;
          path = metadata_path.Path() / url.substr(tls_server.size() + std::string("unstable/").length());
        }
      } else if (url.find("noupdates/") != std::string::npos) {
        path = metadata_path.Path() / url.substr(tls_server.size() + strlen("noupdates/"));
      } else if (url.find("multisec/") != std::string::npos) {
        path = metadata_path.Path() / url.substr(tls_server.size() + strlen("multisec/"));
      } else if (url.find("campaigner/") != std::string::npos) {
        path = metadata_path.Path() / url.substr(tls_server.size() + strlen("campaigner/"));
      } else if (url.find("downloads/") != std::string::npos) {
        path = metadata_path.Path() / url.substr(tls_server.size() + strlen("downloads/"));
      } else {
        path = metadata_path.Path() / url.substr(tls_server.size());
      }
      std::cout << "file served: " << path << "\n";
      if (url.find("repo/timestamp.json") != std::string::npos) {
        if (url.find("noupdates/") != std::string::npos) {
          boost::filesystem::copy_file(metadata_path.Path() / "repo/timestamp_noupdates.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        } else if (url.find("multisec/") != std::string::npos) {
          boost::filesystem::copy_file(metadata_path.Path() / "repo/timestamp_multisec.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        } else {
          boost::filesystem::copy_file(metadata_path.Path() / "repo/timestamp_hasupdates.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        }
        return HttpResponse(Utils::readFile(path), 200, CURLE_OK, "");
      } else if (url.find("repo/targets.json") != std::string::npos) {
        if (url.find("noupdates/") != std::string::npos) {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_noupdates.json"), 200, CURLE_OK, "");
        } else if (url.find("multisec/") != std::string::npos) {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_multisec.json"), 200, CURLE_OK, "");
        } else {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_hasupdates.json"), 200, CURLE_OK, "");
        }
      } else if (url.find("director/targets.json") != std::string::npos) {
        if (url.find("noupdates/") != std::string::npos) {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_noupdates.json"), 200, CURLE_OK, "");
        } else if (url.find("multisec/") != std::string::npos) {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_multisec.json"), 200, CURLE_OK, "");
        } else {
          return HttpResponse(Utils::readFile(path.parent_path() / "targets_hasupdates.json"), 200, CURLE_OK, "");
        }

      } else if (url.find("snapshot.json") != std::string::npos) {
        if (url.find("noupdates/") != std::string::npos) {
          boost::filesystem::copy_file(path.parent_path() / "snapshot_noupdates.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        } else if (url.find("multisec/") != std::string::npos) {
          boost::filesystem::copy_file(path.parent_path() / "snapshot_multisec.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        } else {
          boost::filesystem::copy_file(path.parent_path() / "snapshot_hasupdates.json", path,
                                       boost::filesystem::copy_option::overwrite_if_exists);
        }
        return HttpResponse(Utils::readFile(path), 200, CURLE_OK, "");
      } else if (url.find("campaigner/campaigns") != std::string::npos) {
        return HttpResponse(Utils::readFile(path.parent_path() / "campaigner/campaigns.json"), 200, CURLE_OK, "");
      } else {
        if (boost::filesystem::exists(path)) {
          return HttpResponse(Utils::readFile(path), 200, CURLE_OK, "");
        } else {
          std::cout << "not found: " << path << "\n";
          return HttpResponse({}, 404, CURLE_OK, "");
        }
      }
    }
    return HttpResponse(url, 200, CURLE_OK, "");
  }

  HttpResponse post(const std::string &url, const Json::Value &data) {
    if (url.find("tst149") == 0) {
      if (url == "tst149/devices") {
        devices_count++;
        EXPECT_EQ(data["deviceId"].asString(), "tst149_device_id");
        return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
      } else if (url == "tst149/director/ecus") {
        ecus_count++;
        EXPECT_EQ(data["primary_ecu_serial"].asString(), "tst149_ecu_serial");
        EXPECT_EQ(data["ecus"][0]["hardware_identifier"].asString(), "tst149_hardware_identifier");
        EXPECT_EQ(data["ecus"][0]["ecu_serial"].asString(), "tst149_ecu_serial");
        if (ecus_count == 1) {
          return HttpResponse("{}", 200, CURLE_OK, "");
        } else {
          return HttpResponse("{\"code\":\"ecu_already_registered\"}", 409, CURLE_OK, "");
        }
      } else {
        EXPECT_EQ(0, 1) << "Unexpected post to URL: " << url;
      }
    } else if (url.find("reportqueue") == 0) {
      if (url == "reportqueue/SingleEvent/events") {
        EXPECT_EQ(data[0]["eventType"]["id"], "DownloadComplete");
        EXPECT_EQ(data[0]["event"], "SingleEvent");
        ++events_seen;
        return HttpResponse("", 200, CURLE_OK, "");
      } else if (url.find("reportqueue/MultipleEvents") == 0) {
        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
          EXPECT_EQ(data[i]["eventType"]["id"], "DownloadComplete");
          EXPECT_EQ(data[i]["event"], "MultipleEvents" + std::to_string(events_seen++));
        }
        return HttpResponse("", 200, CURLE_OK, "");
      } else if (url.find("reportqueue/FailureRecovery") == 0) {
        if (data.size() < 10) {
          return HttpResponse("", 400, CURLE_OK, "");
        } else {
          for (int i = 0; i < static_cast<int>(data.size()); ++i) {
            EXPECT_EQ(data[i]["eventType"]["id"], "DownloadComplete");
            EXPECT_EQ(data[i]["event"], "FailureRecovery" + std::to_string(i));
          }
          events_seen = data.size();
          return HttpResponse("", 200, CURLE_OK, "");
        }
      } else {
        EXPECT_EQ(0, 1) << "Unexpected post to URL: " << url;
      }
    }
    Utils::writeFile((test_dir / "post.json").string(), data);
    if (provisioningResponse == ProvisioningResult::kOK) {
      return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
    } else {
      return HttpResponse("", 400, CURLE_OK, "");
    }
  }

  HttpResponse put(const std::string &url, const Json::Value &data) {
    if (url.find("tst149") == 0) {
      if (url == "tst149/core/installed") {
        installed_count++;
        EXPECT_EQ(data.size(), 1);
        EXPECT_EQ(data[0]["name"].asString(), "fake-package");
        EXPECT_EQ(data[0]["version"].asString(), "1.0");
      } else if (url == "tst149/core/system_info") {
        system_info_count++;
        Json::Value hwinfo = Utils::getHardwareInfo();
        EXPECT_EQ(hwinfo["id"].asString(), data["id"].asString());
        EXPECT_EQ(hwinfo["description"].asString(), data["description"].asString());
        EXPECT_EQ(hwinfo["class"].asString(), data["class"].asString());
        EXPECT_EQ(hwinfo["product"].asString(), data["product"].asString());
      } else if (url == "tst149/director/manifest") {
        manifest_count++;
        std::string hash;
        if (manifest_count <= 2) {
          // Check for default initial value of packagemanagerfake.
          hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
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
      } else if (url == "tst149/system_info/network") {
        network_count++;
        Json::Value nwinfo = Utils::getNetworkInfo();
        EXPECT_EQ(nwinfo["local_ipv4"].asString(), data["local_ipv4"].asString());
        EXPECT_EQ(nwinfo["mac"].asString(), data["mac"].asString());
        EXPECT_EQ(nwinfo["hostname"].asString(), data["hostname"].asString());
      } else {
        EXPECT_EQ(0, 1) << "Unexpected put to URL: " << url;
      }
    }

    std::ofstream director_file((test_dir / test_manifest).c_str());
    director_file << data;
    director_file.close();
    return HttpResponse(url, 200, CURLE_OK, "");
  }

  HttpResponse download(const std::string &url, curl_write_callback callback, void *userp) {
    (void)userp;
    std::cout << "URL requested: " << url << "\n";
    const boost::filesystem::path path = metadata_path / "repo/targets" / url.substr(url.rfind("/targets/") + 9);
    std::cout << "file served: " << path << "\n";

    std::string content = Utils::readFile(path.string());
    for (unsigned int i = 0; i < content.size(); ++i) {
      callback(const_cast<char *>(&content[i]), 1, 1, userp);
      if (url.find("downloads/repo/targets/primary_firmware.txt") != std::string::npos) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Simulate big file
      }
    }
    return HttpResponse(content, 200, CURLE_OK, "");
  }

  ProvisioningResult provisioningResponse;
  const std::string test_manifest = "test_aktualizr_manifest.txt";
  const std::string tls_server = "https://tlsserver.com";
  size_t events_seen = 0;
  int devices_count = 0;
  int ecus_count = 0;
  int manifest_count = 0;
  int installed_count = 0;
  int system_info_count = 0;
  int network_count = 0;

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

  int unstable_valid_num{0};
  int unstable_valid_count{0};
};

#endif  // HTTPFAKE_H_
