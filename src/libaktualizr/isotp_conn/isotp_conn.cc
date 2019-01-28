#include "isotp_conn.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <exception>
#include <iostream>

#include <cstring>

#include <chrono>
#include <thread>

#include <boost/algorithm/hex.hpp>

#include "logging/logging.h"

IsoTpSendRecv::IsoTpSendRecv(std::string can_iface_, uint16_t canaddr_rx_, uint16_t canaddr_tx_)
    : can_iface{std::move(can_iface_)}, canaddr_rx{canaddr_rx_}, canaddr_tx{canaddr_tx_} {
  can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);

  if (can_socket < -1) {
    throw std::runtime_error("Unable to open socket");
  }

  struct can_filter filter {};
  filter.can_id = canaddr_rx & 0x7FF;
  filter.can_mask = 0x7FF;
  setsockopt(can_socket, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter));

  struct ifreq ifr {};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  memcpy(ifr.ifr_name, can_iface.c_str(), IFNAMSIZ);

  if (ioctl(can_socket, SIOCGIFINDEX, &ifr) != 0) {
    throw std::runtime_error("Unable to get interface index");
  }

  struct sockaddr_can addr {};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;  // NOLINT

  if (bind(can_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    throw std::runtime_error("Unable to bind socket");
  }

  isotp_shims = isotp_init_shims(nullptr, canSend, nullptr, this);
}

bool IsoTpSendRecv::canSend(uint32_t arbitration_id, const uint8_t* data, uint8_t size, void* private_data) {
  auto* instance = static_cast<IsoTpSendRecv*>(private_data);

  if ((instance == nullptr) || size > 8) {
    return false;
  }

  LOG_TRACE << "Sending CAN message AF: 0x" << std::hex << arbitration_id << "; Data:";
  LOG_TRACE << " " << boost::algorithm::hex(std::string(reinterpret_cast<const char*>(data), size));

  int can_socket = instance->can_socket;

  struct can_frame frame {};

  frame.can_id = arbitration_id;
  frame.can_dlc = size;
  memcpy(frame.data, data, size);

  ssize_t res = write(can_socket, &frame, sizeof(frame));
  if (res < 0) {
    LOG_ERROR << "CAN write error: " << strerror(errno);
    return false;
  }
  if (res != sizeof(frame)) {
    LOG_ERROR << "CAN write error: " << res << " bytes of " << sizeof(frame) << " were sent";
    return false;
  }
  return true;
}

bool IsoTpSendRecv::Send(const std::string& out) {
  IsoTpMessage message_tx = isotp_new_send_message(canaddr_tx, reinterpret_cast<const uint8_t*>(out.c_str()),
                                                   static_cast<uint16_t>(out.length()));
  IsoTpSendHandle send_handle = isotp_send(&isotp_shims, &message_tx, nullptr);
  if (send_handle.completed) {
    if (send_handle.success) {
      return true;
    }
    LOG_ERROR << "ISO/TP message send failed";
    return false;
  } else {
    while (true) {
      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(can_socket, &read_set);

      // struct timeval timeout = {0, 20000};  // 20 ms
      // if (select((can_socket + 1), &read_set, nullptr, nullptr, &timeout) >= 0) {
      if (select((can_socket + 1), &read_set, nullptr, nullptr, nullptr) >= 0) {
        if (FD_ISSET(can_socket, &read_set)) {
          struct can_frame f {};
          ssize_t ret = read(can_socket, &f, sizeof(f));
          if (ret < 0) {
            std::cerr << "Error receiving CAN frame" << std::endl;
            return false;
          }

          LOG_TRACE << "Reveived CAN message in Send method AF: 0x" << std::hex << f.can_id << "; Data:";
          LOG_TRACE << " " << boost::algorithm::hex(std::string(reinterpret_cast<const char*>(f.data), f.can_dlc));

          if (!isotp_receive_flowcontrol(&isotp_shims, &send_handle, static_cast<uint16_t>(f.can_id), f.data,
                                         f.can_dlc)) {
            std::cerr << "IsoTp receiving error" << std::endl;
            return false;
          }

          while (send_handle.to_send != 0) {
            if (send_handle.gap_ms != 0) {
              std::this_thread::sleep_for(std::chrono::milliseconds(send_handle.gap_ms));
            }
            if (send_handle.gap_us != 0) {
              std::this_thread::sleep_for(std::chrono::microseconds(send_handle.gap_us));
            }

            if (!isotp_continue_send(&isotp_shims, &send_handle)) {
              LOG_ERROR << "IsoTp sending error";
              return false;
            }
            if (send_handle.completed) {
              // Wait before (potentially) sending another packet
              if (send_handle.gap_ms != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(send_handle.gap_ms));
              }
              if (send_handle.gap_us != 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(send_handle.gap_us));
              }

              if (send_handle.success) {
                return true;
              }
              LOG_ERROR << "IsoTp send failed";
              return false;
            }
          }

        } else {
          LOG_TRACE << "Timeout on CAN socket";
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
  return false;
}

bool IsoTpSendRecv::Recv(std::string* in) {
  IsoTpReceiveHandle recv_handle = isotp_receive(&isotp_shims, canaddr_tx, canaddr_rx, nullptr);

  while (true) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(can_socket, &read_set);

    // struct timeval timeout = {0, 2000000};  // 20 ms
    // if (select((can_socket + 1), &read_set, nullptr, nullptr, &timeout) >= 0) {
    if (select((can_socket + 1), &read_set, nullptr, nullptr, nullptr) >= 0) {
      if (FD_ISSET(can_socket, &read_set)) {
        struct can_frame f {};
        ssize_t ret = read(can_socket, &f, sizeof(f));
        if (ret < 0) {
          std::cerr << "Error receiving CAN frame" << std::endl;
          return false;
        }

        LOG_TRACE << "Reveived CAN message in Recv method AF: 0x" << std::hex << f.can_id << "; Data:";
        LOG_TRACE << " " << boost::algorithm::hex(std::string(reinterpret_cast<const char*>(f.data), f.can_dlc));
        // std::this_thread::sleep_for(std::chrono::milliseconds(10)); // hack for RIOT to start waiting for flow
        // control
        IsoTpMessage message_rx = isotp_continue_receive(&isotp_shims, &recv_handle, f.can_id, f.data, f.can_dlc);
        if (message_rx.completed && recv_handle.completed) {
          if (!recv_handle.success) {
            std::cerr << "IsoTp receiving error" << std::endl;
            return false;
          }
          *in = std::string(reinterpret_cast<const char*>(message_rx.payload), static_cast<size_t>(message_rx.size));
          return true;
        }
      } else {
        LOG_TRACE << "Timeout on CAN socket";
        *in = "";
        return false;
      }
    } else {
      LOG_ERROR << "Select failed";
      return false;
    }
  }
}
