#include <gtest/gtest.h>

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "config/config.h"
#include "http/httpclient.h"
#include "logging/logging.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage.h"
#include "utilities/utils.h"

/**
 * \verify{\tst{49}} Check that aktualizr fails on expired metadata
 */

/**
 * \verify{\tst{52}} Check that aktualizr fails on bad threshold
 */

// This is a class solely for the purpose of being a FRIEND_TEST to
// SotaUptaneClient. The name is carefully constructed for this purpose.
class Uptane_Vector_Test {
 private:
  static bool match_error(const Json::Value& error, const Uptane::Exception& e) {
    std::cout << "name: " << e.getName() << std::endl;
    std::cout << "e.what: " << e.what() << std::endl;
    std::cout << "1: " << error["director"]["update"]["err_msg"].asString() << std::endl;
    std::cout << "2: " << error["director"]["targets"][e.getName()]["err_msg"].asString() << std::endl;
    std::cout << "3: " << error["image_repo"]["update"]["err_msg"].asString() << std::endl;
    std::cout << "4: " << error["image_repo"]["targets"][e.getName()]["err_msg"].asString() << std::endl;
    if (error["director"]["update"]["err_msg"].asString() == e.what() ||
        error["director"]["targets"][e.getName()]["err_msg"].asString() == e.what() ||
        error["image_repo"]["update"]["err_msg"].asString() == e.what() ||
        error["image_repo"]["targets"][e.getName()]["err_msg"].asString() == e.what()) {
      return true;
    }
    std::cout << "aktualizr failed with unmatched exception " << typeid(e).name() << ": " << e.what() << "\n";
    return false;
  }

 public:
  static bool run_test(const std::string& test_name, const Json::Value& vector, const std::string& port) {
    std::cout << "VECTOR: " << vector;
    TemporaryDirectory temp_dir;
    Config config;
    config.provision.primary_ecu_serial = "test_primary_ecu_serial";
    config.provision.primary_ecu_hardware_id = "test_primary_hardware_id";
    config.uptane.director_server = "http://127.0.0.1:" + port + "/" + test_name + "/director";
    config.uptane.repo_server = "http://127.0.0.1:" + port + "/" + test_name + "/image_repo";
    config.storage.path = temp_dir.Path();
    config.storage.uptane_metadata_path = BasedPath(port + "/aktualizr_repos");
    config.pacman.type = PackageManager::kNone;
    logger_set_threshold(boost::log::trivial::trace);

    try {
      auto storage = INvStorage::newStorage(config.storage);
      HttpClient http;
      Uptane::Manifest uptane_manifest{config, storage};
      std::shared_ptr<event::Channel> events_channel{new event::Channel};

      Bootloader bootloader(config.bootloader);
      ReportQueue report_queue(config, http);
      SotaUptaneClient uptane_client(config, events_channel, uptane_manifest, storage, http, bootloader, report_queue);
      Uptane::EcuSerial ecu_serial(config.provision.primary_ecu_serial);
      Uptane::HardwareIdentifier hw_id(config.provision.primary_ecu_hardware_id);
      uptane_client.hw_ids.insert(std::make_pair(ecu_serial, hw_id));
      uptane_client.installed_images[ecu_serial] = "test_filename";
      if (!uptane_client.uptaneIteration()) {
        throw uptane_client.getLastException();
      }

    } catch (const Uptane::Exception& e) {
      return match_error(vector, e);
    } catch (const std::exception& e) {
      std::cout << "aktualizr failed with unrecognized exception " << typeid(e).name() << ": " << e.what() << "\n";
      return false;
    }

    if (vector["director"]["update"]["is_success"].asBool() && vector["image_repo"]["update"]["is_success"].asBool()) {
      for (Json::ValueConstIterator it = vector["director"]["targets"].begin();
           it != vector["director"]["targets"].end(); ++it) {
        if (!(*it)["is_success"].asBool()) {
          std::cout << "aktualizr did not fail as expected.\n";
          return false;
        }
      }
      for (Json::ValueConstIterator it = vector["image_repo"]["targets"].begin();
           it != vector["image_repo"]["targets"].end(); ++it) {
        if (!(*it)["is_success"].asBool()) {
          std::cout << "aktualizr did not fail as expected.\n";
          return false;
        }
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
};

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 3) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh!\n";
    return 1;
  }

  HttpClient http_client;
  /* Use ports to distinguish both the server connection and local storage so
   * that parallel runs of this code don't cause problems that are difficult to
   * debug. */
  const std::string port = argv[2];
  const std::string address = "http://127.0.0.1:" + port + "/";
  const Json::Value json_vectors = http_client.get(address, HttpInterface::kNoLimit).getJson();
  int passed = 0;
  int failed = 0;
  for (Json::ValueConstIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    std::cout << "Running test vector " << (*it).asString() << "\n";
    while (true) {
      HttpResponse response = http_client.post(address + (*it).asString() + "/step", Json::Value());
      if (response.http_status_code == 204) {
        break;
      }

      bool pass = Uptane_Vector_Test::run_test((*it).asString(), response.getJson(), port);
      std::cout << "Finished test vector " << (*it).asString() << "\n";
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
