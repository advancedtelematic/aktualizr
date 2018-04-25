#include <iostream>

#include <boost/filesystem.hpp>
#include <utility>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "test_isotp_interface.h"
#include "utilities/utils.h"

#define HW_ID_DID 0x0001
#define ECU_SERIAL_DID 0x0002

TestIsotpInterface::TestIsotpInterface(const unsigned int loglevel, uint32_t canid, std::string canif)
    : loglevel_(loglevel), canId(canid), canIface(std::move(canif)) {
  can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);

  if (can_socket < -1) {
    throw std::runtime_error("Unable to open socket");
  }

  struct can_filter filter {};
  filter.can_id = canId & 0x1F;
  filter.can_mask = 0x1F;
  setsockopt(can_socket, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter));

  struct ifreq ifr {};
  strncpy(ifr.ifr_name, canIface.c_str(), IFNAMSIZ);  // NOLINT

  if (ioctl(can_socket, SIOCGIFINDEX, &ifr) != 0) {
    throw std::runtime_error("Unable to get interface index");
  }

  struct sockaddr_can addr {};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;  // NOLINT

  if (bind(can_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    throw std::runtime_error("Unable to bind socket");
  }

  isotp_shims = isotp_init_shims(nullptr, isoTpSend, nullptr, this);

  populateEcus();
}

std::string TestIsotpInterface::apiVersion() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying api version" << std::endl;
  }
  return "1";
}

std::string TestIsotpInterface::listEcus() {
  if (loglevel_ == 4) {
    std::cerr << "Displaying list of ecus:" << std::endl;
  }

  std::string res;
  std::map<std::pair<std::string, std::string>, uint32_t>::iterator it;
  for (it = ecus.begin(); it != ecus.end(); ++it) {
    const std::string& hw_id = it->first.first;
    const std::string& ecu_serial = it->first.second;
    res += ((hw_id + "\t") += ecu_serial) += "\n";
  }

  return res;
}

TestIsotpInterface::InstallStatus TestIsotpInterface::installSoftware(const std::string& hardware_id,
                                                                      const std::string& ecu_id,
                                                                      const std::string& firmware) {
  if (loglevel_ == 4) {
    std::cerr << "Installing hardware_id: " << hardware_id << ", ecu_id: " << ecu_id << " firmware_path: " << firmware
              << std::endl;
  }

  std::map<std::pair<std::string, std::string>, uint32_t>::const_iterator it =
      ecus.find(std::make_pair(hardware_id, ecu_id));
  if (it == ecus.end()) {
    std::cerr << "ECU with hardware_id " << hardware_id << " and serial number " << ecu_id << " was not found."
              << std::endl;
    return InstallFailureNotModified;
  }
  uint32_t id = it->second;

  if (!boost::filesystem::exists(firmware)) {
    std::cerr << "Firmware not found on path " << firmware << std::endl;
    return InstallFailureNotModified;
  }
  std::string firmware_content = Utils::readFile(firmware);

  std::string payload;
  std::string resp;

  payload.push_back(0x10);  // DiagnosticSessionControl
  payload.push_back(0x02);  // ProgrammingSession

  struct timeval timeout = {0, 100000};  // 100ms
  if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
    std::cerr << "Error entering programming mode" << std::endl;
    return InstallFailureNotModified;
  }

  uint32_t start_addr = 0x00008000;  // TODO: get from ECU
  uint32_t size = firmware_content.size();
  payload.clear();
  payload.push_back(0x31);  // RoutineControl
  payload.push_back(0x01);  // Start routine
  payload.push_back(0xFF);  // Erase
  payload.push_back(0x00);
  payload.push_back(start_addr >> 24);
  payload.push_back(start_addr >> 16);
  payload.push_back(start_addr >> 8);
  payload.push_back(start_addr);
  payload.push_back(size >> 24);
  payload.push_back(size >> 16);
  payload.push_back(size >> 8);
  payload.push_back(size);

  if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
    std::cerr << "Error erasing flash" << std::endl;
    return InstallFailureNotModified;
  }

  payload.clear();
  payload.push_back(0x34);  // RequestDownload
  payload.push_back(0x00);  // Raw data
  payload.push_back(0x44);  // 4 bytes long address and size
  payload.push_back(start_addr >> 24);
  payload.push_back(start_addr >> 16);
  payload.push_back(start_addr >> 8);
  payload.push_back(start_addr);
  payload.push_back(size >> 24);
  payload.push_back(size >> 16);
  payload.push_back(size >> 8);
  payload.push_back(size);

  if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
    std::cerr << "RequestDownload failed" << std::endl;
    return InstallFailureModified;
  }
  if (resp[0] != (0x34 | 0x40)) {
    std::cerr << "Unexpected response on RequestDownload" << std::endl;
    return InstallFailureModified;
  }
  if (((resp[1] >> 4) & 0x0f) > 4) {
    std::cerr << "Block size doesn't fit in 4 bytes" << std::endl;
    return InstallFailureModified;
  }
  uint32_t block_size = 0;
  for (uint8_t i = 0; i < ((resp[1] >> 4) & 0x0f); i++) {
    block_size |= resp[2 + i];
    block_size <<= 8;
  }

  if (block_size > kMaxBlockSize) {
    block_size = kMaxBlockSize;
  }

  uint8_t seqn = 1;

  for (size_t i = 0; i < size; i += block_size) {
    payload.clear();
    payload.push_back(0x36);  // TransferData
    payload.push_back(seqn++);

    int len = (size - i >= block_size) ? block_size : size - i;

    for (int j = 0; j < len; j++) {
      payload.push_back(firmware_content[i + j]);
    }

    if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
      std::cerr << "TransferData failed" << std::endl;
      return InstallFailureModified;
    }
  }
  payload.clear();
  payload.push_back(0x37);  // RequestTransferExit
  if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
    std::cerr << "RequestTransferExit failed" << std::endl;
    return InstallFailureModified;
  }

  payload.clear();
  payload.push_back(0x11);  // ECUReset
  payload.push_back(0x01);  // Hard reset
  if (!sendRecvUds(payload, &resp, canId, id, &timeout) || resp.empty()) {
    std::cerr << "RequestTransferExit failed" << std::endl;
    return InstallFailureModified;
  }

  return InstallationSuccessful;
}

uint16_t TestIsotpInterface::makeCanAf(uint16_t sa, uint16_t ta) { return ((sa << 5) & 0x3E0) | (ta & 0x1F); }

bool TestIsotpInterface::isoTpSend(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size,
                                   void* private_data) {
  auto* instance = static_cast<TestIsotpInterface*>(private_data);

  if ((instance == nullptr) || size > 8) {
    return false;
  }

  if (instance->loglevel_ == 4) {
    std::cerr << "Sending CAN message AF: 0x" << std::hex << arbitration_id << "; Data:";
    for (int i = 0; i < size; i++) {
      std::cerr << " " << std::hex << static_cast<int>(data[i]);
    }
    std::cerr << std::endl;
  }

  int can_socket = instance->can_socket;

  struct can_frame frame {};

  frame.can_id = arbitration_id;
  frame.can_dlc = size;
  memcpy(frame.data, data, size);

  ssize_t res = write(can_socket, &frame, sizeof(frame));
  if (res < 0) {
    std::cerr << "CAN write error: " << strerror(errno) << std::endl;
    return false;
  }
  if (res != sizeof(frame)) {
    std::cerr << "CAN write error: " << res << " bytes of " << sizeof(frame) << " were sent" << std::endl;
    return false;
  }
  return true;
}

bool TestIsotpInterface::sendRecvUds(const std::string& out, std::string* in, uint16_t sa, uint16_t ta,
                                     struct timeval* to) {
  if (out.empty()) {
    return false;
  }

  IsoTpMessage message =
      isotp_new_send_message(makeCanAf(sa, ta), reinterpret_cast<const uint8_t*>(out.c_str()), out.length());
  IsoTpSendHandle send_handle = isotp_send(&isotp_shims, &message, nullptr);
  if (send_handle.completed) {
    if (!send_handle.success) {
      std::cerr << "Message send failed" << std::endl;
      return false;
    }
  } else {
    while (true) {
      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(can_socket, &read_set);

      struct timeval timeout = *to;
      if (select((can_socket + 1), &read_set, nullptr, nullptr, &timeout) >= 0) {
        if (FD_ISSET(can_socket, &read_set)) {
          struct can_frame f {};
          int ret = read(can_socket, &f, sizeof(f));
          if (ret < 0) {
            std::cerr << "Error receiving CAN frame" << std::endl;
            return false;
          }

          if (!isotp_receive_flowcontrol(&isotp_shims, &send_handle, f.can_id, f.data, f.can_dlc)) {
            std::cerr << "IsoTp receiving error" << std::endl;
            return false;
          }

          while (send_handle.to_send != 0) {
            // TODO: fix firmware
            usleep(100000);
            if (!isotp_continue_send(&isotp_shims, &send_handle)) {
              std::cerr << "IsoTp sending error" << std::endl;
              return false;
            }
            if (send_handle.completed) {
              if (send_handle.success) {
                break;  // proceed to waiting for response
              }
              std::cerr << "IsoTp send failed" << std::endl;
              return false;
            }
          }

        } else {
          if (loglevel_ == 4) {
            std::cerr << "Timeout on CAN socket" << std::endl;
          }
          *in = "";
          return true;
        }
        if (send_handle.completed) {
          break;
        }
      } else {
        std::cerr << "Select failed" << std::endl;
        return false;
      }
    }
  }

  IsoTpReceiveHandle recv_handle = isotp_receive(&isotp_shims, makeCanAf(ta, sa), nullptr);

  while (true) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(can_socket, &read_set);

    struct timeval timeout = *to;
    if (select((can_socket + 1), &read_set, nullptr, nullptr, &timeout) >= 0) {
      if (FD_ISSET(can_socket, &read_set)) {
        struct can_frame f {};
        int ret = read(can_socket, &f, sizeof(f));
        if (ret < 0) {
          std::cerr << "Error receiving CAN frame" << std::endl;
          return false;
        }

        IsoTpMessage message = isotp_continue_receive(&isotp_shims, &recv_handle, f.can_id, f.data, f.can_dlc);
        if (message.completed && recv_handle.completed) {
          if (!recv_handle.success) {
            std::cerr << "IsoTp receiving error" << std::endl;
            return false;
          }
          *in = std::string(reinterpret_cast<const char*>(message.payload), static_cast<size_t>(message.size));
          return true;
        }
      } else {
        if (loglevel_ == 4) {
          std::cerr << "Timeout on CAN socket" << std::endl;
        }
        *in = "";
        return true;
      }
    } else {
      std::cerr << "Select failed" << std::endl;
      return false;
    }
  }
}

void TestIsotpInterface::populateEcus() {
  uint8_t payload_hwid[3] = {0x22, (HW_ID_DID >> 8), (HW_ID_DID & 0xFF)};
  uint8_t payload_serial[3] = {0x22, (ECU_SERIAL_DID >> 8), (ECU_SERIAL_DID & 0xFF)};

  struct timeval timeout = {0, 20000};  // 20ms

  for (uint8_t id = 0x01; id <= 0x1f; id++) {
    std::string resp;
    std::string hwid;
    if (!sendRecvUds(std::string(reinterpret_cast<const char*>(payload_hwid), static_cast<size_t>(3)), &resp, canId, id,
                     &timeout)) {
      std::cerr << "Error sending request for HW ID for id " << static_cast<int>(id) << std::endl;
      continue;
    }
    if (resp.empty()) {  // timeout
      if (loglevel_ == 4) {
        std::cerr << "Request for HW ID for id " << static_cast<int>(id) << " timed out" << std::endl;
      }
      continue;
    }

    if (resp.length() < 3) {
      std::cerr << "Invalid response to request for HW ID" << std::endl;
      continue;
    }

    if (resp[0] != 0x62) {
      std::cerr << "Invalid response id " << static_cast<int>(resp[0]) << " when " << 0x62 << " was expected"
                << std::endl;
      continue;
    }

    if (((resp[1] << 8) | resp[2]) != HW_ID_DID) {
      std::cerr << "Invalid DID " << ((resp[1] << 8) | resp[2]) << " when " << HW_ID_DID << " was expected"
                << std::endl;
      continue;
    }

    hwid = resp.substr(3);
    if (!sendRecvUds(std::string(reinterpret_cast<const char*>(payload_serial), static_cast<size_t>(3)), &resp, canId,
                     id, &timeout)) {
      std::cerr << "Error sending request for ECU serial for id " << static_cast<int>(id) << std::endl;
      continue;
    }

    if (resp.empty()) {  // timeout
      if (loglevel_ == 4) {
        std::cerr << "Request for HW ID for id " << static_cast<int>(id) << " timed out" << std::endl;
      }
      continue;
    }

    if (resp.length() < 3) {
      std::cerr << "Invalid response to request for HW ID" << std::endl;
      continue;
    }

    if (resp[0] != 0x62) {
      std::cerr << "Invalid response id " << static_cast<int>(resp[0]) << " when " << 0x62 << " was expected"
                << std::endl;
      continue;
    }

    if (((resp[1] << 8) | resp[2]) != ECU_SERIAL_DID) {
      std::cerr << "Invalid DID " << ((resp[1] << 8) | resp[2]) << " when " << ECU_SERIAL_DID << " was expected"
                << std::endl;
      continue;
    }
    ecus[std::make_pair(hwid, resp.substr(3))] = id;
  }
}
