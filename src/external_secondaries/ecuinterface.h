#include <string>

class ECUInterface {
 public:
  enum InstallStatus {
    InstallationSuccessful = 0,
    FirmawareIsInvalid,
    InstallFailureNotModified,
    InstallFailureModified
  };
  ECUInterface(unsigned int loglevel) : loglevel_(loglevel) {}
  std::string apiVersion();
  std::string listEcus();
  InstallStatus installSoftware(const std::string &hardware_id, const std::string &ecu_id, const std::string &firmware);

 private:
  unsigned int loglevel_;
};