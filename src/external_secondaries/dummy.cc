#include <iostream>

#include "ecuinterface.h"

std::string ECUInterface::apiVersion() { return "1"; }

std::string ECUInterface::listEcus() { return "hardware_id ecu_id"; }

ECUInterface::InstallStatus ECUInterface::installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                                          const std::string &firmware) {
  std::cout << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
            << "\n";
  return InstallationSuccessfull;
}
