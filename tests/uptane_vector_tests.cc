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
#include "storage/invstorage.h"
#include "utilities/utils.h"

/**
 * TODO: Convert this into proper unit tests?
 * Check that aktualizr fails on expired metadata.
 * RecordProperty("zephyr_key", "REQ-150,TST-49");
 * Check that aktualizr fails on bad threshold.
 * RecordProperty("zephyr_key", "REQ-153,TST-52");
 */

// This is a class solely for the purpose of being a FRIEND_TEST to
// SotaUptaneClient. The name is carefully constructed for this purpose.
class Uptane_Vector_Test {
 private:
  static bool match_error(const Json::Value& error, const Uptane::Exception& e) {
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
  static bool run_test(const std::string& test_name, const std::string& address) {
    TemporaryDirectory temp_dir;
    Config config;
    config.provision.primary_ecu_serial = "test_primary_ecu_serial";
    config.provision.primary_ecu_hardware_id = "test_primary_hardware_id";
    config.uptane.director_server = address + test_name + "/director";
    config.uptane.repo_server = address + test_name + "/image_repo";
    config.storage.path = temp_dir.Path();
    config.storage.uptane_metadata_path = BasedPath(temp_dir.Path() / "metadata");
    config.pacman.type = PackageManager::kNone;
    logger_set_threshold(boost::log::trivial::trace);

    auto storage = INvStorage::newStorage(config.storage);
    Uptane::Manifest uptane_manifest{config, storage};
    auto uptane_client = SotaUptaneClient::newDefaultClient(config, storage);
    Uptane::EcuSerial ecu_serial(config.provision.primary_ecu_serial);
    Uptane::HardwareIdentifier hw_id(config.provision.primary_ecu_hardware_id);
    uptane_client->hw_ids.insert(std::make_pair(ecu_serial, hw_id));
    uptane_client->installed_images[ecu_serial] = "test_filename";

    HttpClient http_client;
    while (true) {
      HttpResponse response = http_client.post(address + test_name + "/step", Json::Value());
      if (response.http_status_code == 204) {
        return true;
      }
      const auto vector(response.getJson());
      std::cout << "VECTOR: " << vector;

      bool should_fail = false;
      if (vector["director"]["update"]["is_success"].asBool() == false ||
          vector["image_repo"]["update"]["is_success"].asBool() == false) {
        should_fail = true;
      } else {
        for (const auto& t : vector["director"]["targets"]) {
          if (t["is_success"].asBool() == false) {
            should_fail = true;
            break;
          }
        }
        for (const auto& t : vector["image_repo"]["targets"]) {
          if (t["is_success"].asBool() == false) {
            should_fail = true;
            break;
          }
        }
      }

      try {
        if (!uptane_client->uptaneIteration()) {
          if (should_fail) {
            throw uptane_client->getLastException();
          }
          std::cout << "uptaneIteration unexpectedly failed.\n";
          return false;
        }
        std::vector<Uptane::Target> updates;
        if (!uptane_client->uptaneOfflineIteration(&updates, nullptr)) {
          std::cout << "uptaneOfflineIteration unexpectedly failed.\n";
          return false;
        }
        if (updates.size()) {
          if (!uptane_client->downloadImages(updates)) {
            if (should_fail) {
              throw uptane_client->getLastException();
            }
            std::cout << "downloadImages unexpectedly failed.\n";
            return false;
          }
        }

      } catch (const Uptane::Exception& e) {
        if (match_error(vector, e)) {
          continue;
        }
        return false;
      } catch (const std::exception& e) {
        std::cout << "aktualizr failed with unrecognized exception " << typeid(e).name() << ": " << e.what() << "\n";
        return false;
      }

      if (should_fail) {
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
    return false;
  }
};

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 2) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh!\n";
    return 1;
  }

  HttpClient http_client;
  /* Use ports to distinguish both the server connection and local storage so
   * that parallel runs of this code don't cause problems that are difficult to
   * debug. */
  const std::string port = argv[1];
  const std::string address = "http://127.0.0.1:" + port + "/";
  const Json::Value json_vectors = http_client.get(address, HttpInterface::kNoLimit).getJson();
  int passed = 0;
  int failed = 0;
  for (Json::ValueConstIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    std::cout << "Running test vector " << (*it).asString() << "\n";

    const bool pass = Uptane_Vector_Test::run_test((*it).asString(), address);
    std::cout << "Finished test vector " << (*it).asString() << "\n";
    if (pass) {
      passed++;
      std::cout << "TEST: PASS\n";
    } else {
      failed++;
      std::cout << "TEST: FAIL\n";
    }
  }
  std::cout << "\n\n\nPASSED TESTS: " << passed << "\n";
  std::cout << "FAILED TESTS: " << failed << "\n";
  return failed;
}
