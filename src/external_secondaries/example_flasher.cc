#include <unistd.h>
#include <iostream>

#include <boost/filesystem.hpp>

#include "example_flasher.h"
#include "utilities/utils.h"

std::string filename;
std::string serial;

ExampleFlasher::ExampleFlasher(const unsigned int loglevel) : loglevel_(loglevel) {
  // Use /var/sota if it is available and accessible to the current user.
  // Generally this is true for devices but not hosts.
  if (boost::filesystem::exists("/var/sota")) {
    if (access("/var/sota", R_OK | W_OK | X_OK) != 0) {
      filename = "/tmp/example_serial";
    } else {
      filename = "/var/sota/example_serial";
    }
  } else {
    filename = "/tmp/example_serial";
  }

  if (boost::filesystem::exists(filename)) {
    serial = Utils::readFile(filename);
  } else {
    serial = Utils::randomUuid();
    Utils::writeFile(filename, serial);
  }
}

std::string ExampleFlasher::apiVersion() {
  if (loglevel_ == 0) {
    std::cerr << "Displaying api version:\n";
  }
  return "1";
}

std::string ExampleFlasher::listEcus() {
  if (loglevel_ == 0) {
    std::cerr << "Displaying list of ecus:\n";
  }
  return std::string("example1 ") + serial + "\n" + "example2\n";
}

ExampleFlasher::InstallStatus ExampleFlasher::installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                                              const std::string &firmware) {
  if (loglevel_ == 0) {
    std::cerr << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
              << "\n";
  }
  if (hardware_id == "example1" && ecu_id == serial) {
    return InstallationSuccessful;
  }
  if (hardware_id == "example2") {
    return InstallationSuccessful;
  }
  return InstallFailureNotModified;
}
