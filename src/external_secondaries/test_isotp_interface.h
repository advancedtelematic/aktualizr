#ifndef TEST_ISOTP_INTERFACE_H_
#define TEST_ISOTP_INTERFACE_H_

#include <stdint.h>
#include <map>
#include <string>

#include "isotp/isotp.h"

#include "ecuinterface.h"

const uint32_t kDefaultCanId = 0x03;
const uint32_t kMaxBlockSize = 1024;  // 1K
const std::string kDefaultCanIf = "can0";
class TestIsotpInterface : public ECUInterface {
 public:
  TestIsotpInterface(unsigned int loglevel, uint32_t canid = kDefaultCanId, const std::string& canif = kDefaultCanIf);
  std::string apiVersion();
  std::string listEcus();
  InstallStatus installSoftware(const std::string& hardware_id, const std::string& ecu_id, const std::string& firmware);

 private:
  // (hardware_id, ecu_serial) -> CAN address
  std::map<std::pair<std::string, std::string>, uint32_t> ecus;

  void populateEcus();
  uint16_t makeCanAf(uint16_t sa, uint16_t ta);
  bool sendRecvUds(const std::string& out, std::string* in, uint16_t sa, uint16_t ta, struct timeval* to);

  static bool isoTpSend(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size, void* private_data);

  int can_socket;

  int loglevel_;
  uint32_t canId;
  std::string canIface;
  IsoTpShims isotp_shims;
};

#endif  // TEST_ISOTP_INTERFACE_H_
