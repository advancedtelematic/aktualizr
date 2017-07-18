#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include <gtest/gtest.h>
#include <string>
#include "config.h"
#include "httpclient.h"
#include "logger.h"
#include "utils.h"

#include "uptane/uptanerepository.h"

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

bool run_test(const std::string& test_name, const Json::Value& vector) {
  std::cout << "VECTOR: " << vector;
  HttpClient http_client;
  Config config;
  std::string url_director = "http://127.0.0.1:8080/" + test_name + "/director";
  std::string url_image = "http://127.0.0.1:8080/" + test_name + "/image_repo";
  config.uptane.director_server = url_director;
  config.uptane.repo_server = url_image;
  config.uptane.metadata_path = "/tmp/aktualizr_repos";
  config.ostree.os = "myos";
  config.ostree.sysroot = "./sysroot";
  boost::filesystem::remove_all(config.uptane.metadata_path / "director");
  boost::filesystem::remove_all(config.uptane.metadata_path / "repo");

  try {
    Uptane::Repository repo(config);
    repo.updateRoot(Uptane::Version(1));
    repo.getNewTargets();

  } catch (Uptane::Exception e) {
    return match_error(vector, &e);
  } catch (std::exception ex) {
    std::cout << "Undefined exception: " << ex.what();
    return false;
  }

  if (vector["director"]["update"]["is_success"].asBool() && vector["image_repo"]["update"]["is_success"].asBool()) {
    for (Json::ValueIterator it = vector["director"]["targets"].begin(); it != vector["director"]["targets"].end();
         ++it) {
      if (!(*it)["is_success"].asBool()) return false;
    }
    for (Json::ValueIterator it = vector["image_repo"]["targets"].begin(); it != vector["image_repo"]["targets"].end();
         ++it) {
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
  loggerInit();
  loggerSetSeverity(LVL_maximum);
  // loggerSetSeverity(LVL_minimum);

  if (argc != 2) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh\n";
    return 1;
  }
  HttpClient http_client;
  Json::Value json_vectors = http_client.get("http://127.0.0.1:8080/").getJson();
  int passed = 0;
  int failed = 0;
  for (Json::ValueIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    std::cout << "Running test vector " << (*it).asString() << "\n";
    while (true) {
      HttpResponse response = http_client.post("http://127.0.0.1:8080/" + (*it).asString() + "/step", Json::Value());
      if (response.http_status_code == 204) {
        break;
      }

      bool pass = run_test((*it).asString(), response.getJson());
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
