#include <iostream>

#include <boost/filesystem.hpp>

#include "ecuinterface.h"
#include "utils.h"

const std::string filename = "/tmp/example_serial";
std::string serial;

ECUInterface::ECUInterface(const unsigned int loglevel) : loglevel_(loglevel) {
  if (boost::filesystem::exists(filename)) {
    serial = Utils::readFile(filename);
  } else {
    serial = Utils::randomUuid();
    Utils::writeFile(filename, serial);
  }
}

std::string ECUInterface::apiVersion() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying api version:\n";
  }
  return "1";
}

std::string ECUInterface::listEcus() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying list of ecus:\n";
  }
  return std::string("example1 ") + serial + "\n" + "example2\n";
}

ECUInterface::InstallStatus ECUInterface::installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                                          const std::string &firmware) {
  if (loglevel_ == 4) {
    std::cerr << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
              << "\n";
  }
  if (hardware_id == "example1" && ecu_id == serial) {
    return InstallationSuccessful;
  } else if (hardware_id == "example2") {
    return InstallationSuccessful;
  } else {
    return InstallFailureNotModified;
  }
}
