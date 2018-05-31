#include "ipsecondarydiscovery.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <chrono>

#include "AKIpUptaneMes.h"
#include "asn1/asn1_message.h"
#include "logging/logging.h"

std::vector<Uptane::SecondaryConfig> IpSecondaryDiscovery::discover() {
  std::vector<Uptane::SecondaryConfig> secondaries;

  {
    int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
      LOG_ERROR << "Error of creating socket: " << std::strerror(errno);
      return secondaries;
    }
    SocketHandle hdl(new int(socket_fd));
    socket_hdl = std::move(hdl);
  }

  struct sockaddr_in6 recv_addr {};
  recv_addr.sin6_family = AF_INET6;
  recv_addr.sin6_port = htons(0);
  recv_addr.sin6_addr = IN6ADDR_ANY_INIT;
  if (bind(*socket_hdl, reinterpret_cast<struct sockaddr *>(&recv_addr), sizeof recv_addr) < 0) {
    LOG_ERROR << "Error of binding socket: " << std::strerror(errno);
    return secondaries;
  }

  if (!sendRequest()) {
    return secondaries;
  }
  return waitDevices();
}

std::vector<Uptane::SecondaryConfig> IpSecondaryDiscovery::waitDevices() {
  LOG_INFO << "Waiting " << config_.ipdiscovery_wait_seconds << "s for discovery responses...";
  std::vector<Uptane::SecondaryConfig> secondaries;

  auto start_time = std::chrono::system_clock::now();
  struct timeval tv {};
  tv.tv_sec = config_.ipdiscovery_wait_seconds;
  tv.tv_usec = 0;
  setsockopt(*socket_hdl, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));

  char rbuf[2000] = {};
  int recieved = 0;
  struct sockaddr_storage sec_address {};
  socklen_t sec_addr_len = sizeof(sec_address);

  memset(&sec_address, 0, sizeof(sockaddr_storage));

  while ((recieved = recvfrom(*socket_hdl, rbuf, sizeof(rbuf) - 1, 0, reinterpret_cast<struct sockaddr *>(&sec_address),
                              &sec_addr_len)) != -1) {
    Uptane::SecondaryConfig conf;
    AKIpUptaneMes_t *m = nullptr;
    asn_dec_rval_t res = ber_decode(nullptr, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void **>(&m), rbuf, recieved);
    // Note that ber_decode allocates *m even on failure, so this must always be done
    Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

    if (res.code != RC_OK) {
      LOG_ERROR << "Failed to parse discovery response message";
      LOG_ERROR << Utils::toBase64(std::string(rbuf, recieved));
      continue;
    }

    if (msg->present() != AKIpUptaneMes_PR_discoveryResp) {
      LOG_ERROR << "Unexpected message type:" << msg->present();
      continue;
    }
    auto resp = msg->discoveryResp();
    conf.ecu_serial = ToString(resp->ecuSerial);
    conf.ecu_hardware_id = ToString(resp->hwId);
    conf.ip_addr = sec_address;
    Utils::setSocketPort(&conf.ip_addr, htons(resp->port));

    LOG_INFO << "Found secondary:" << conf.ecu_serial << " " << conf.ecu_hardware_id;
    conf.secondary_type = Uptane::SecondaryType::kIpUptane;

    secondaries.push_back(conf);

    auto now = std::chrono::system_clock::now();
    int left_seconds =
        config_.ipdiscovery_wait_seconds - std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    if (left_seconds <= 0) {
      break;
    }
    tv.tv_sec = left_seconds;
    setsockopt(*socket_hdl, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
  }
  return secondaries;
}

bool IpSecondaryDiscovery::sendRequest() {
  LOG_INFO << "Sending UDP discovery packet to " << config_.ipdiscovery_host << ":" << config_.ipdiscovery_port;

  int broadcast = 1;
  if (setsockopt(*socket_hdl, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast) < 0) {
    LOG_ERROR << "Could not setup socket for broadcast: " << std::strerror(errno);
    return false;
  }
  struct sockaddr_in6 sendaddr {};
  sendaddr.sin6_family = AF_INET6;
  sendaddr.sin6_port = htons(config_.ipdiscovery_port);
  inet_pton(AF_INET6, config_.ipdiscovery_host.c_str(), &sendaddr.sin6_addr);

  Asn1Message::Ptr m(Asn1Message::Empty());
  m->present(AKIpUptaneMes_PR_discoveryReq);
  auto req_msg = m->discoveryReq();
  req_msg->port = config_.ipuptane_port;  // Only needed while we have bi-directional connections

  std::string buffer;
  asn_enc_rval_t enc = der_encode(&asn_DEF_AKIpUptaneMes, &m->msg_, Asn1StringAppendCallback, &buffer);
  if (enc.encoded == -1) {
    LOG_ERROR << "ASN.1 Serialization failed";
    return false;
  }

  int numbytes = sendto(*socket_hdl, buffer.c_str(), buffer.size(), 0, reinterpret_cast<struct sockaddr *>(&sendaddr),
                        sizeof sendaddr);
  if (numbytes == -1) {
    LOG_ERROR << "Could not send discovery request: " << std::strerror(errno);
    return false;
  }
  return true;
}
