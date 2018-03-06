#ifndef ECUINTERFACE_H_
#define ECUINTERFACE_H_

#include <string>

class ECUInterface {
 public:
  enum InstallStatus {
    InstallationSuccessful = 0,
    FirmawareIsInvalid,
    InstallFailureNotModified,
    InstallFailureModified
  };
  virtual ~ECUInterface() {}
  virtual std::string apiVersion() = 0;
  virtual std::string listEcus() = 0;
  virtual InstallStatus installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                        const std::string &firmware) = 0;
};

#endif  // ECUINTERFACE_H_
