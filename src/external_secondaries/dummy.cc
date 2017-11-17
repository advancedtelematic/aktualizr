#include <iostream>

#include "ecuinterface.h"

std::string ECUInterface::apiVersion() { return "1"; }

std::string ECUInterface::listEcus() { return "msp430\n"; }

ECUInterface::InstallStatus ECUInterface::installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                                          const std::string &firmware) {
  std::cerr << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
            << "\n";
  return InstallationSuccessful;
}
