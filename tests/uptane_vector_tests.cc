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
#include "logger.h"
#include "utils.h"

#include "uptane/uptanerepository.h"

bool match_error(Json::Value errors, Uptane::Exception* e) {
  std::string repos;
  std::string messages;
  for (Json::ValueIterator it = errors.begin(); it != errors.end(); it++) {
    repos += std::string(repos.size() ? " or " : "") + "'" + it.key().asString() + "'";
    messages += std::string(messages.size() ? " or " : "") + "'" + (*it)["error_msg"].asString() + "'";
    std::string exc_name = (*it)["error_msg"].asString();
    if (boost::starts_with(exc_name, "SecurityException") && typeid(Uptane::SecurityException) != typeid(*e))
      continue;
    else if (boost::starts_with(exc_name, "TargetHashMismatch") && typeid(Uptane::TargetHashMismatch) != typeid(*e))
      continue;
    else if (boost::starts_with(exc_name, "OversizedTarget") && typeid(Uptane::OversizedTarget) != typeid(*e))
      continue;
    else if (boost::starts_with(exc_name, "MissingRepo") && typeid(Uptane::MissingRepo) != typeid(*e))
      continue;
    else if (boost::starts_with(exc_name, "ExpiredMetadata") && typeid(Uptane::ExpiredMetadata) != typeid(*e))
      continue;

    if (it.key().asString() == e->getName()) {
      if ((*it)["error_msg"].asString() == e->what()) {
        return true;
      }
    }
  }
  std::cout << "Exception " << typeid(*e).name() << "\n";
  std::cout << "Message '" << e->what() << "' should be match: " << messages << "\n";
  std::cout << "and Repo '" << e->getName() << "' should be match: " << repos << "\n";
  return false;
}

bool run_test(Json::Value vector) {
  Config config;
  std::string url_director = "http://127.0.0.1:8080/" + vector["repo"].asString() + "/director/repo";
  std::string url_image = "http://127.0.0.1:8080/" + vector["repo"].asString() + "/repo/repo";
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
    if (vector["is_success"].asBool()) {
      std::cout << "Exception " << typeid(e).name() << " happened, but shouldn't\n";
      std::cout << "exception message: " << e.what() << "\n";
      return false;
    }
    return match_error(vector["errors"], &e);

  } catch (...) {
    std::cout << "Undefined exception\n";
    return false;
  }

  if (vector["is_success"].asBool()) {
    return true;
  } else {
    std::cout << "No exceptions happen, but expects\n";
    return false;
  }
}

int main(int argc, char* argv[]) {
  loggerInit();
  loggerSetSeverity(LVL_maximum);

  if (argc != 2) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh\n";
    return 1;
  }
  Json::Value json_vectors = Utils::parseJSONFile(std::string(argv[1]));
  int passed = 0;
  int failed = json_vectors["vectors"].size();
  for (Json::ValueIterator it = json_vectors["vectors"].begin(); it != json_vectors["vectors"].end(); it++) {
    std::cout << "Running test vector " << (*it)["repo"].asString() << "\n";
    bool pass = run_test(*it);
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
