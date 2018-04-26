#include <gtest/gtest.h>

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "config/config.h"
#include "http/httpclient.h"
#include "logging/logging.h"
#include "storage/fsstorage.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

bool match_error(Json::Value error, Uptane::Exception* e) {
  if (error["director"]["update"]["err_msg"].asString() == e->what() ||
      error["director"]["targets"][e->getName()]["err_msg"].asString() == e->what() ||
      error["image_repo"]["update"]["err_msg"].asString() == e->what() ||
      error["image_repo"]["targets"][e->getName()]["err_msg"].asString() == e->what()) {
    return true;
  }
  std::cout << "Exception " << typeid(*e).name() << "\n";
  std::cout << "Message '" << e->what() << "\n";
  return false;
}

bool run_test(const std::string& test_name, const Json::Value& vector, const std::string& port) {
  std::cout << "VECTOR: " << vector;
  TemporaryDirectory temp_dir;
  HttpClient http_client;
  Config config;
  config.uptane.director_server = "http://127.0.0.1:" + port + "/" + test_name + "/director";
  config.uptane.repo_server = "http://127.0.0.1:" + port + "/" + test_name + "/image_repo";
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = port + "/aktualizr_repos";
  config.pacman.os = "myos";
  config.pacman.sysroot = "./sysroot";

  try {
    auto storage = INvStorage::newStorage(config.storage);
    HttpClient http;
    Uptane::Repository repo(config, storage, http);
    repo.updateRoot(Uptane::Version(1));
    repo.getMeta();
    repo.getTargets();

  } catch (Uptane::Exception e) {
    return match_error(vector, &e);
  } catch (const std::exception& ex) {
    std::cout << "Undefined exception: " << ex.what() << "\n";
    return false;
  }

  if (vector["director"]["update"]["is_success"].asBool() && vector["image_repo"]["update"]["is_success"].asBool()) {
    for (Json::ValueConstIterator it = vector["director"]["targets"].begin(); it != vector["director"]["targets"].end();
         ++it) {
      if (!(*it)["is_success"].asBool()) return false;
    }
    for (Json::ValueConstIterator it = vector["image_repo"]["targets"].begin();
         it != vector["image_repo"]["targets"].end(); ++it) {
      if (!(*it)["is_success"].asBool()) return false;
    }

    return true;
  } else {
    std::cout << "No exceptions happen, but expects ";
    if (!vector["director"]["update"]["is_success"].asBool()) {
      std::cout << "exception from director: '" << vector["director"]["update"]["err"]
                << " with message: " << vector["director"]["update"]["err_msg"] << "\n";
    } else if (!vector["image_repo"]["update"]["is_success"].asBool()) {
      std::cout << "exception from image_repo: '" << vector["image_repo"]["update"]["err"]
                << " with message: " << vector["image_repo"]["update"]["err_msg"] << "\n";
    }

    return false;
  }
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 3) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh\n";
    return 1;
  }

  HttpClient http_client;
  /* Use ports to distinguish both the server connection and local storage so
   * that parallel runs of this code don't cause problems that are difficult to
   * debug. */
  const std::string port = argv[2];
  const std::string address = "http://127.0.0.1:" + port + "/";
  const Json::Value json_vectors = http_client.get(address).getJson();
  int passed = 0;
  int failed = 0;
  for (Json::ValueConstIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    std::cout << "Running test vector " << (*it).asString() << "\n";
    while (true) {
      HttpResponse response = http_client.post(address + (*it).asString() + "/step", Json::Value());
      if (response.http_status_code == 204) {
        break;
      }

      bool pass = run_test((*it).asString(), response.getJson(), port);
      if (pass) {
        passed++;
        std::cout << "TEST: PASS\n";
      } else {
        failed++;
        std::cout << "TEST: FAIL\n";
      }
    }
  }
  std::cout << "\n\n\nPASSED TESTS: " << passed << "\n";
  std::cout << "FAILED TESTS: " << failed << "\n";
  return failed;
}
