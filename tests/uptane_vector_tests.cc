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

std::string address;

class VectorWrapper {
 public:
  VectorWrapper(Json::Value vector) : vector_(std::move(vector)) {}

  bool matchError(const Uptane::Exception& e) {
    if (vector_["director"]["update"]["err_msg"].asString() == e.what() ||
        vector_["director"]["targets"][e.getName()]["err_msg"].asString() == e.what() ||
        vector_["image_repo"]["update"]["err_msg"].asString() == e.what() ||
        vector_["image_repo"]["targets"][e.getName()]["err_msg"].asString() == e.what()) {
      return true;
    }
    std::cout << "aktualizr failed with unmatched exception " << typeid(e).name() << ": " << e.what() << "\n";
    std::cout << "Expected error: " << vector_ << "\n";
    return false;
  }

  bool shouldFail() {
    bool should_fail = false;
    if (!vector_["director"]["update"]["is_success"].asBool() ||
        !vector_["image_repo"]["update"]["is_success"].asBool()) {
      should_fail = true;
    } else {
      for (const auto& t : vector_["director"]["targets"]) {
        if (!t["is_success"].asBool()) {
          should_fail = true;
          break;
        }
      }
      for (const auto& t : vector_["image_repo"]["targets"]) {
        if (!t["is_success"].asBool()) {
          should_fail = true;
          break;
        }
      }
    }
    return should_fail;
  }

  void printExpectedFailure() {
    std::cout << "No exceptions occurred, but expected ";
    if (!vector_["director"]["update"]["is_success"].asBool()) {
      std::cout << "exception from director: '" << vector_["director"]["update"]["err"]
                << " with message: " << vector_["director"]["update"]["err_msg"] << "\n";
    } else if (!vector_["image_repo"]["update"]["is_success"].asBool()) {
      std::cout << "exception from image_repo: '" << vector_["image_repo"]["update"]["err"]
                << " with message: " << vector_["image_repo"]["update"]["err_msg"] << "\n";
    } else {
      std::cout << "an exception while fetching targets metadata.\n";
    }
  }

 private:
  Json::Value vector_;
};

class UptaneVector : public ::testing::TestWithParam<std::string> {};

/**
 * Check that aktualizr fails on expired metadata.
 * RecordProperty("zephyr_key", "REQ-150,TST-49");
 * Check that aktualizr fails on bad threshold.
 * RecordProperty("zephyr_key", "REQ-153,TST-52");
 */
TEST_P(UptaneVector, Test) {
  const std::string test_name = GetParam();
  std::cout << "Running test vector " << test_name << "\n";

  TemporaryDirectory temp_dir;
  Config config;
  config.provision.primary_ecu_serial = "test_primary_ecu_serial";
  config.provision.primary_ecu_hardware_id = "test_primary_hardware_id";
  config.uptane.director_server = address + test_name + "/director";
  config.uptane.repo_server = address + test_name + "/image_repo";
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = BasedPath(temp_dir.Path() / "metadata");
  config.pacman.type = PACKAGE_MANAGER_NONE;
  logger_set_threshold(boost::log::trivial::trace);

  auto storage = INvStorage::newStorage(config.storage);
  auto uptane_client = std_::make_unique<SotaUptaneClient>(config, storage);
  Uptane::EcuSerial ecu_serial(config.provision.primary_ecu_serial);
  Uptane::HardwareIdentifier hw_id(config.provision.primary_ecu_hardware_id);
  uptane_client->hw_ids.insert(std::make_pair(ecu_serial, hw_id));
  uptane_client->primary_ecu_serial_ = ecu_serial;
  Uptane::EcuMap ecu_map{{ecu_serial, hw_id}};
  Uptane::Target target("test_filename", ecu_map, {{Uptane::Hash::Type::kSha256, "sha256"}}, 1, "");
  storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kCurrent);

  HttpClient http_client;
  while (true) {
    HttpResponse response = http_client.post(address + test_name + "/step", Json::Value());
    if (response.http_status_code == 204) {
      return;
    }
    const auto vector_json(response.getJson());
    std::cout << "VECTOR: " << vector_json;
    VectorWrapper vector(vector_json);

    bool should_fail = vector.shouldFail();

    try {
      /* Fetch metadata from the director.
       * Check metadata from the director.
       * Identify targets for known ECUs.
       * Fetch metadata from the images repo.
       * Check metadata from the images repo.
       *
       * It would be simpler to just call fetchMeta() here, but that calls
       * putManifestSimple(), which will fail here. */
      if (!uptane_client->uptaneIteration(nullptr, nullptr)) {
        ASSERT_TRUE(should_fail) << "uptaneIteration unexpectedly failed.";
        throw uptane_client->getLastException();
      }
      result::UpdateCheck updates = uptane_client->checkUpdates();
      if (updates.status == result::UpdateStatus::kError) {
        ASSERT_TRUE(should_fail) << "checkUpdates unexpectedly failed.";
        throw uptane_client->getLastException();
      }
      if (updates.ecus_count > 0) {
        /* Download a binary package.
         * Verify a binary package. */
        result::Download result = uptane_client->downloadImages(updates.updates);
        if (result.status != result::DownloadStatus::kSuccess) {
          ASSERT_TRUE(should_fail) << "downloadImages unexpectedly failed.";
          throw uptane_client->getLastException();
        }
      }

    } catch (const Uptane::Exception& e) {
      ASSERT_TRUE(vector.matchError(e)) << "libaktualizr threw a different exception than expected!";
      continue;
    } catch (const std::exception& e) {
      FAIL() << "libaktualizr failed with unrecognized exception " << typeid(e).name() << ": " << e.what();
    }

    if (should_fail) {
      vector.printExpectedFailure();
      FAIL();
    }
  }
  FAIL() << "Step sequence unexpectedly aborted.";
}

std::vector<std::string> GetVectors() {
  HttpClient http_client;
  const Json::Value json_vectors = http_client.get(address, HttpInterface::kNoLimit).getJson();
  std::vector<std::string> vectors;
  for (Json::ValueConstIterator it = json_vectors.begin(); it != json_vectors.end(); it++) {
    vectors.emplace_back((*it).asString());
  }
  return vectors;
}

INSTANTIATE_TEST_SUITE_P(UptaneVectorSuite, UptaneVector, ::testing::ValuesIn(GetVectors()));

int main(int argc, char* argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc < 2) {
    std::cerr << "This program is intended to be run from run_vector_tests.sh!\n";
    return 1;
  }

  /* Use ports to distinguish both the server connection and local storage so
   * that parallel runs of this code don't cause problems that are difficult to
   * debug. */
  const std::string port = argv[1];
  address = "http://127.0.0.1:" + port + "/";

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
