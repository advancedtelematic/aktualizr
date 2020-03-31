#ifndef UPTANE_ISOTP_SEND_RECV_H_
#define UPTANE_ISOTP_SEND_RECV_H_

#include <cstdint>
#include <string>
#include "isotp/isotp.h"

class IsoTpSendRecv {
 public:
  IsoTpSendRecv(std::string can_iface_, uint16_t canaddr_rx_, uint16_t canaddr_tx_);
  bool Send(const std::string& out);
  bool SendRecv(const std::string& out, std::string* in) { return Send(out) && Recv(in); }

 private:
  std::string can_iface;
  uint16_t canaddr_rx;
  uint16_t canaddr_tx;
  int can_socket;
  IsoTpShims isotp_shims{};

  bool Recv(std::string* in);
  static bool canSend(uint32_t arbitration_id, const uint8_t* data, uint8_t size, void* private_data);
};

#endif  // UPTANE_ISOTP_SEND_RECV_H_
