#ifndef METAFAKE_H_
#define METAFAKE_H_

#include <fstream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include "logging/logging.h"
#include "uptane_repo.h"
#include "utilities/utils.h"

class MetaFake {
 public:
  MetaFake(const boost::filesystem::path &meta_dir_in)
      : meta_dir(meta_dir_in), work_dir(meta_dir / "fake_meta"), repo(work_dir, "2021-07-04T16:33:27Z", "id0") {
    repo.generateRepo(KeyType::kED25519);
    backup();
    create_image();
    create_testData();
  }

 private:
  void create_testData(void) {
    boost::filesystem::path file_name;
    std::string hwid;
    Delegation delegation;

    // add image for "has update" meta
    file_name = "dummy_firmware.txt";
    repo.addImage(work_dir / file_name, file_name, "dummy", "", delegation);

    file_name = "primary_firmware.txt";
    hwid = "primary_hw";
    repo.addImage(work_dir / file_name, file_name, hwid, "", delegation);
    repo.addTarget(file_name.string(), hwid, "CA:FE:A6:D2:84:9D", "");

    file_name = "secondary_firmware.txt";
    hwid = "secondary_hw";
    repo.addImage(work_dir / file_name, file_name, hwid, "", delegation);
    repo.addTarget(file_name.string(), hwid, "secondary_ecu_serial", "");

    repo.signTargets();
    rename("_hasupdates");

    // add image for "no update" meta
    restore();

    file_name = "dummy_firmware.txt";
    repo.addImage(work_dir / file_name, file_name, "dummy", "", delegation);

    repo.signTargets();
    rename("_noupdates");

    // add image for "multi secondary ecu" meta
    restore();

    file_name = "dummy_firmware.txt";
    repo.addImage(work_dir / file_name, file_name, "dummy", "", delegation);

    file_name = "secondary_firmware.txt";
    hwid = "sec_hw1";
    repo.addImage(work_dir / file_name, file_name, hwid, "", delegation);
    repo.addTarget(file_name.string(), hwid, "sec_serial1", "");

    file_name = "secondary_firmware2.txt";
    hwid = "sec_hw2";
    repo.addImage(work_dir / file_name, file_name, hwid, "", delegation);
    repo.addTarget(file_name.string(), hwid, "sec_serial2", "");

    repo.signTargets();
    rename("_multisec");

    // copy meta to work_dir
    Utils::copyDir(work_dir / ImageRepo::dir, meta_dir / "repo");
    Utils::copyDir(work_dir / DirectorRepo::dir, meta_dir / "director");
    if (!boost::filesystem::exists(meta_dir / "campaigner") &&
        boost::filesystem::is_directory("tests/test_data/campaigner")) {
      Utils::copyDir("tests/test_data/campaigner", meta_dir / "campaigner");
    }
  }

  void create_image(void) {
    std::string content = "Just to increment timestamp, ignore it\n";
    Utils::writeFile(work_dir / "dummy_firmware.txt", content);
    content = "This is a dummy firmware file (should never be downloaded)\n";
    Utils::writeFile(work_dir / "primary_firmware.txt", content);
    content = "This is content";
    Utils::writeFile(work_dir / "secondary_firmware.txt", content);
    content = "This is more content\n";
    Utils::writeFile(work_dir / "secondary_firmware2.txt", content);
  }

  void backup(void) {
    backup_files.push_back(work_dir / DirectorRepo::dir / "targets.json");
    backup_content.push_back(Utils::readFile(backup_files[0], false));

    backup_files.push_back(work_dir / ImageRepo::dir / "snapshot.json");
    backup_content.push_back(Utils::readFile(backup_files[1], false));

    backup_files.push_back(work_dir / ImageRepo::dir / "targets.json");
    backup_content.push_back(Utils::readFile(backup_files[2], false));

    backup_files.push_back(work_dir / ImageRepo::dir / "timestamp.json");
    backup_content.push_back(Utils::readFile(backup_files[3], false));
  }

  void restore(void) {
    for (unsigned int i = 0; i < backup_files.size(); i++) {
      Utils::writeFile(backup_files[i], backup_content[i]);
    }
  }

  void rename(const std::string &appendix) {
    for (unsigned int i = 0; i < backup_files.size(); i++) {
      boost::filesystem::rename(backup_files[i],
                                (backup_files[i].parent_path() / backup_files[i].stem()).string() + appendix + ".json");
    }
  }

 private:
  boost::filesystem::path meta_dir;
  boost::filesystem::path work_dir;
  UptaneRepo repo;
  std::vector<boost::filesystem::path> backup_files;
  std::vector<std::string> backup_content;
};

#endif  // METAFAKE_H

// vim: set tabstop=2 shiftwidth=2 expandtab:
