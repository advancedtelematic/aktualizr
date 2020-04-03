#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "config/config.h"
#include "httpfake.h"
#include "primary/aktualizr.h"
#include "uptane_test_common.h"
#include "virtualsecondary.h"

boost::filesystem::path fake_meta_dir;

void verifyEcus(TemporaryDirectory& temp_dir, std::vector<std::string> expected_ecus) {
  const Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), expected_ecus.size());
  for (const Json::Value& ecu : ecu_data["ecus"]) {
    auto found = std::find(expected_ecus.begin(), expected_ecus.end(), ecu["ecu_serial"].asString());
    if (found != expected_ecus.end()) {
      expected_ecus.erase(found);
    } else {
      FAIL() << "Unknown ECU in registration data: " << ecu["ecu_serial"].asString();
    }
  }
  EXPECT_EQ(expected_ecus.size(), 0);
}

class HttpFakeRegistration : public HttpFake {
 public:
  HttpFakeRegistration(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "noupdates", meta_dir_in) {}

  HttpResponse post(const std::string& url, const Json::Value& data) override {
    if (url.find("/director/ecus") != std::string::npos) {
      registration_count += 1;
      EXPECT_EQ(data["primary_ecu_serial"].asString(), "CA:FE:A6:D2:84:9D");
      EXPECT_EQ(data["ecus"][0]["ecu_serial"].asString(), "CA:FE:A6:D2:84:9D");
      EXPECT_EQ(data["ecus"][0]["hardware_identifier"].asString(), "primary_hw");
    }
    return HttpFake::post(url, data);
  }

  unsigned int registration_count{0};
};

/*
 * Add a Secondary via API, register the ECUs, then add another, and re-register.
 */
TEST(Aktualizr, AddSecondary) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeRegistration>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);

  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  Primary::VirtualSecondaryConfig ecu_config = UptaneTestCommon::altVirtualConfiguration(temp_dir.Path());
  ImageReader image_reader = std::bind(&INvStorage::openTargetFile, storage.get(), std::placeholders::_1);
  aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config, image_reader));
  aktualizr.Initialize();

  std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "ecuserial3", "secondary_ecu_serial"};
  verifyEcus(temp_dir, expected_ecus);
  EXPECT_EQ(http->registration_count, 1);

  ecu_config.ecu_serial = "ecuserial4";
  auto sec4 = std::make_shared<Primary::VirtualSecondary>(ecu_config);
  aktualizr.AddSecondary(sec4);
  aktualizr.Initialize();
  expected_ecus.push_back(ecu_config.ecu_serial);
  verifyEcus(temp_dir, expected_ecus);
  EXPECT_EQ(http->registration_count, 2);
}

/*
 * Add a Secondary via API, register the ECUs, remove one, and re-register.
 */
TEST(Aktualizr, RemoveSecondary) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeRegistration>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::VirtualSecondaryConfig ecu_config = UptaneTestCommon::altVirtualConfiguration(temp_dir.Path());
    aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config));
    aktualizr.Initialize();

    std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "ecuserial3", "secondary_ecu_serial"};
    verifyEcus(temp_dir, expected_ecus);
    EXPECT_EQ(http->registration_count, 1);
  }

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "secondary_ecu_serial"};
    verifyEcus(temp_dir, expected_ecus);
    EXPECT_EQ(http->registration_count, 2);
  }
}

/*
 * Add a Secondary via API, register the ECUs, replace one, and re-register.
 */
TEST(Aktualizr, ReplaceSecondary) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeRegistration>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::VirtualSecondaryConfig ecu_config = UptaneTestCommon::altVirtualConfiguration(temp_dir.Path());
    aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config));
    aktualizr.Initialize();

    std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "ecuserial3", "secondary_ecu_serial"};
    verifyEcus(temp_dir, expected_ecus);
    EXPECT_EQ(http->registration_count, 1);
  }

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::VirtualSecondaryConfig ecu_config = UptaneTestCommon::altVirtualConfiguration(temp_dir.Path());
    ecu_config.ecu_serial = "ecuserial4";
    aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config));
    aktualizr.Initialize();

    std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "ecuserial4", "secondary_ecu_serial"};
    verifyEcus(temp_dir, expected_ecus);
    EXPECT_EQ(http->registration_count, 2);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  TemporaryDirectory tmp_dir;
  fake_meta_dir = tmp_dir.Path();
  MetaFake meta_fake(fake_meta_dir);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
