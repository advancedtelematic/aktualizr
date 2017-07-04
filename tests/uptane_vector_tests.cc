#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include <string>
#include "config.h"
#include "httpclient.h"
#include "logger.h"
#include "utils.h"

#include "uptane/uptanerepository.h"

bool match_error(Json::Value error, Uptane::Exception* e) {
  if (error["update"]["err_msg"].asString() == e->what()) {
    return true;
  }
  std::cout << "Exception " << typeid(*e).name() << "\n";
  std::cout << "Message '" << e->what() << "' should be match: " << error["update"]["err_msg"].asString() << "\n";
  return false;
}

bool run_test(const std::string& test_name) {
  HttpClient http_client;
  Json::Value vector = http_client.post("http://127.0.0.1:8080/" + test_name + "/step", Json::Value()).getJson();
  // std::cout << "STEP: " << vector;

  Config config;
  std::string url_director = "http://127.0.0.1:8080/" + test_name + "/director";
  std::string url_image = "http://127.0.0.1:8080/" + test_name + "/image_repo";
  config.uptane.director_server = url_director;
  config.uptane.repo_server = url_image;
  config.uptane.metadata_path = "/tmp/aktualizr_repos";
  boost::filesystem::remove_all(config.uptane.metadata_path / "director");
  boost::filesystem::remove_all(config.uptane.metadata_path / "repo");

  try {
    Uptane::Repository repo(config);
    repo.updateRoot();
    repo.getNewTargets();

  } catch (Uptane::Exception e) {
    if (!vector["director"]["update"]["is_success"].asBool()) {
      return match_error(vector["director"], &e);
    } else if (!vector["image_repo"]["update"]["is_success"].asBool()) {
      return match_error(vector["image_repo"], &e);
    }
    return false;

  } catch (...) {
    std::cout << "Undefined exception\n";
    return false;
  }

  if (vector["director"]["update"]["is_success"].asBool() && vector["image_repo"]["update"]["is_success"].asBool()) {
    return true;
  } else {
    std::cout << "No exceptions happen, but expects ";
    if (!vector["director"]["update"]["is_success"].asBool()) {
      std::cout << "exception from director: '" << vector["director"]["update"]["err"]
                << " with message: " << vector["director"]["update"]["err_msg"] << "\n";
    } else if (!vector["image_repo"]["update"]["is_success"].asBool()) {
      std::cout << "exception from director: '" << vector["image_repo"]["update"]["err"]
                << " with message: " << vector["image_repo"]["update"]["err_msg"] << "\n";
    }

    return false;
  }
}

int main(int argc, char* argv[]) {
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
  int failed = json_vectors.size();
  for (Json::ValueIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    std::cout << "Running test vector " << (*it).asString() << "\n";
    bool pass = run_test((*it).asString());
    if (pass) {
      passed++;
      failed--;
      std::cout << "TEST: PASS\n";
    } else {
      std::cout << "TEST: FAIL\n";
    }
  }
  std::cout << "\n\n\nPASSED TESTS: " << passed << "\n";
  std::cout << "FAILED TESTS: " << failed << "\n";
  return failed;
}
