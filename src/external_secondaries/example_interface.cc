#include <iostream>

#include "ecuinterface.h"

std::string ECUInterface::apiVersion() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying api version\n";
  }
  return "1";
}

std::string ECUInterface::listEcus() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying list of ecus:\n";
  }
  return "msp430 123456\n";
}

ECUInterface::InstallStatus ECUInterface::installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                                          const std::string &firmware) {
  if (loglevel_ == 4) {
    std::cerr << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
              << "\n";
  }
  if (hardware_id != "msp430" || ecu_id != "123456") {
    return InstallFailureNotModified;
  } else {
    return InstallationSuccessful;
  }
}
