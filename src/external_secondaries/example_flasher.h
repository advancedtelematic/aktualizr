#ifndef EXAMPLE_FLASHER_H_
#define EXAMPLE_FLASHER_H_

#include "ecuinterface.h"

class ExampleFlasher : public ECUInterface {
 public:
  ~ExampleFlasher() override = default;
  explicit ExampleFlasher(unsigned int loglevel);
  std::string apiVersion() override;
  std::string listEcus() override;
  InstallStatus installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                const std::string &firmware) override;

 private:
  unsigned int loglevel_;
};

#endif  // EXAMPLE_FLASHER_H_
