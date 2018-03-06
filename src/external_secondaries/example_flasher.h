#ifndef EXAMPLE_FLASHER_H_
#define EXAMPLE_FLASHER_H_

#include "ecuinterface.h"

class ExampleFlasher : public ECUInterface {
 public:
  virtual ~ExampleFlasher() {}
  ExampleFlasher(const unsigned int loglevel);
  virtual std::string apiVersion();
  virtual std::string listEcus();
  virtual InstallStatus installSoftware(const std::string &hardware_id, const std::string &ecu_id,
                                        const std::string &firmware);

 private:
  int loglevel_;
};

#endif  // EXAMPLE_FLASHER_H_
