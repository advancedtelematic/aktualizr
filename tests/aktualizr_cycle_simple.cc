#include <gtest/gtest.h>

#include <future>
#include <iostream>
#include <string>
#include <thread>

#include "config/config.h"
#include "logging/logging.h"
#include "primary/aktualizr.h"
#include "storage/sqlstorage.h"

int updateOneCycle(const boost::filesystem::path &storage_dir, const std::string &server) {
  Config conf;
  conf.pacman.type = PACKAGE_MANAGER_NONE;
  conf.pacman.fake_need_reboot = true;
  conf.provision.device_id = "device_id";
  conf.provision.ecu_registration_endpoint = server + "/director/ecus";
  conf.tls.server = server;
  conf.uptane.director_server = server + "/director";
  conf.uptane.repo_server = server + "/repo";
  conf.uptane.key_type = KeyType::kED25519;
  conf.provision.server = server;
  conf.provision.provision_path = "tests/test_data/cred.zip";
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = storage_dir;
  conf.bootloader.reboot_sentinel_dir = storage_dir;
  conf.postUpdateValues();
  logger_set_threshold(boost::log::trivial::debug);

  {
    Aktualizr aktualizr(conf);

    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    if (update_result.status != result::UpdateStatus::kUpdatesAvailable) {
      LOG_ERROR << "no update available";
      return 0;
    }

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    if (download_result.status != result::DownloadStatus::kSuccess) {
      LOG_ERROR << "download failed";
      return 1;
    }

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    if (install_result.ecu_reports.size() != 1) {
      LOG_ERROR << "install failed";
      return 1;
    }
    if (install_result.ecu_reports[0].install_res.result_code.num_code != data::ResultCode::Numeric::kNeedCompletion) {
      return 1;
    }
  }

  // do "reboot" and finalize
  boost::filesystem::remove(conf.bootloader.reboot_sentinel_dir / conf.bootloader.reboot_sentinel_name);

  {
    Aktualizr aktualizr(conf);

    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    if (update_result.status != result::UpdateStatus::kNoUpdatesAvailable) {
      LOG_ERROR << "finalize failed";
      return 1;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  logger_init();

  if (argc != 3) {
    std::cerr << "Error: " << argv[0] << " requires the path to the storage directory "
              << "and url of uptane server\n";
    return EXIT_FAILURE;
  }
  boost::filesystem::path storage_dir = argv[1];
  std::string server = argv[2];

  return updateOneCycle(storage_dir, server);
}
