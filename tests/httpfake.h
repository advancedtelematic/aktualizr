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
#include "logging/logging.h"
#include "utilities/utils.h"

enum class ProvisioningResult { kOK, kFailure };

class HttpFake : public HttpInterface {
 public:
  // old style HttpFake with centralized multi repo and url rewriting
  HttpFake(const boost::filesystem::path &test_dir_in, std::string flavor = "",
           const boost::filesystem::path &repo_dir_in = "tests/test_data/repo")
      : test_dir(test_dir_in), flavor_(std::move(flavor)) {
    Utils::copyDir(repo_dir_in / "repo" / "image", metadata_path.Path() / "repo");
    Utils::copyDir(repo_dir_in / "repo" / "director", metadata_path.Path() / "director");
    if (boost::filesystem::is_directory(repo_dir_in / "campaigner")) {
      Utils::copyDir(repo_dir_in / "campaigner", metadata_path.Path() / "campaigner");
    }
  }

  virtual ~HttpFake() {}

  void setCerts(const std::string &ca, CryptoSource ca_source, const std::string &cert, CryptoSource cert_source,
                const std::string &pkey, CryptoSource pkey_source) override {
    (void)ca;
    (void)ca_source;
    (void)cert;
    (void)cert_source;
    (void)pkey;
    (void)pkey_source;
  }

  // rewrite xxx/yyy.json to xxx/yyy_flavor.json
  bool rewrite(std::string &url, const std::string &pattern) {
    size_t pat_pos = url.find(pattern);
    if (pat_pos == std::string::npos) {
      return false;
    }
    size_t ext_pos = pattern.find(".json");
    if (ext_pos == std::string::npos) {
      LOG_ERROR << "Invalid pattern";
      url = "";
      return false;
    }

    url.replace(pat_pos + ext_pos, std::string(".json").size(), "_" + flavor_ + ".json");
    return true;
  }

  virtual HttpResponse handle_event(const std::string &url, const Json::Value &data) {
    (void)url;
    (void)data;
    // do something in child instances
    return HttpResponse("", 400, CURLE_OK, "");
  }

  HttpResponse get(const std::string &url, int64_t maxsize) override {
    (void)maxsize;
    std::cout << "URL requested: " << url << "\n";

    std::string new_url = url;
    if (!flavor_.empty()) {
      rewrite(new_url, "director/targets.json") || rewrite(new_url, "repo/timestamp.json") ||
          rewrite(new_url, "repo/targets.json") || rewrite(new_url, "snapshot.json");

      if (new_url != url) {
        std::cout << "Rewritten to: " << new_url << "\n";
      }
    }

    const boost::filesystem::path path = metadata_path.Path() / new_url.substr(tls_server.size());

    std::cout << "file served: " << path << "\n";

    if (boost::filesystem::exists(path)) {
      return HttpResponse(Utils::readFile(path), 200, CURLE_OK, "");
    } else {
      std::cout << "not found: " << path << "\n";
      return HttpResponse({}, 404, CURLE_OK, "");
    }
  }

  HttpResponse post(const std::string &url, const Json::Value &data) override {
    if (url.find("/devices") != std::string::npos || url.find("/director/ecus") != std::string::npos || url.empty()) {
      LOG_ERROR << "OK create device";
      Utils::writeFile((test_dir / "post.json").string(), data);
      if (provisioningResponse == ProvisioningResult::kOK) {
        return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
      } else {
        return HttpResponse("", 400, CURLE_OK, "");
      }
    } else if (url.find("/events") != std::string::npos) {
      return handle_event(url, data);
    }

    return HttpResponse("", 400, CURLE_OK, "");
  }

  HttpResponse put(const std::string &url, const Json::Value &data) override {
    last_manifest = data;
    return HttpResponse(url, 200, CURLE_OK, "");
  }

  std::future<HttpResponse> downloadAsync(const std::string &url, curl_write_callback write_cb,
                                          curl_xferinfo_callback progress_cb, void *userp, curl_off_t from,
                                          CurlHandler *easyp) override {
    (void)userp;
    (void)from;
    (void)easyp;
    (void)progress_cb;

    std::cout << "URL requested: " << url << "\n";
    const boost::filesystem::path path = metadata_path.Path() / url.substr(tls_server.size());
    std::cout << "file served: " << path << "\n";

    std::promise<HttpResponse> resp_promise;
    auto resp_future = resp_promise.get_future();
    std::thread(
        [path, write_cb, progress_cb, userp, url](std::promise<HttpResponse> promise) {
          std::string content = Utils::readFile(path.string());
          for (unsigned int i = 0; i < content.size(); ++i) {
            write_cb(const_cast<char *>(&content[i]), 1, 1, userp);
            progress_cb(userp, 0, 0, 0, 0);
            if (url.find("downloads/repo/targets/primary_firmware.txt") != std::string::npos) {
              std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Simulate big file
            }
          }
          promise.set_value(HttpResponse(content, 200, CURLE_OK, ""));
        },
        std::move(resp_promise))
        .detach();

    return resp_future;
  }

  HttpResponse download(const std::string &url, curl_write_callback write_cb, curl_xferinfo_callback progress_cb,
                        void *userp, curl_off_t from) override {
    return downloadAsync(url, write_cb, progress_cb, userp, from, nullptr).get();
  }

  const std::string tls_server = "https://tlsserver.com";
  ProvisioningResult provisioningResponse{ProvisioningResult::kOK};
  Json::Value last_manifest;

 protected:
  boost::filesystem::path test_dir;
  TemporaryDirectory metadata_path;
  std::string flavor_;

 private:
  /**
   * These are here to catch a common programming error where a Json::Value is
   * implicitly constructed from a std::string. By having an private overload
   * that takes string (and with no implementation), this will fail during
   * compilation.
   */
  HttpResponse post(const std::string &url, const std::string data);
  HttpResponse put(const std::string &url, const std::string data);
};

#endif  // HTTPFAKE_H_
