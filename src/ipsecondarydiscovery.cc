#include "ipsecondarydiscovery.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <chrono>

#include "logging.h"

std::vector<Uptane::SecondaryConfig> IpSecondaryDiscovery::discover() {
  std::vector<Uptane::SecondaryConfig> secondaries;

  {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
      LOG_ERROR << "Error of creating socket: " << std::strerror(errno);
      return secondaries;
    }
    SocketHandle hdl(new int(socket_fd));
    socket_hdl = std::move(hdl);
  }

  struct sockaddr_in recv_addr {};
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_port = htons(0);
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(*socket_hdl, (struct sockaddr *)&recv_addr, sizeof recv_addr) < 0) {
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
  struct timeval tv;
  tv.tv_sec = config_.ipdiscovery_wait_seconds;
  tv.tv_usec = 0;
  setsockopt(*socket_hdl, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

  char rbuf[2000] = {};
  int recieved = 0;
  while ((recieved = recv(*socket_hdl, rbuf, sizeof(rbuf) - 1, 0)) != -1) {
    std::string data(std::move(rbuf), recieved);
    try {
      int type = 0;
      Uptane::SecondaryConfig conf;
      asn1::Deserializer des(data);
      des >> asn1::seq >> asn1::implicit<kAsn1Integer>(type) >> asn1::implicit<kAsn1Utf8String>(conf.ecu_serial) >>
          asn1::implicit<kAsn1Utf8String>(conf.ecu_hardware_id) >> asn1::restseq;
      if (type == AKT_DISCOVERY_RESP) {
        LOG_INFO << "Found secondary:" << conf.ecu_serial << " " << conf.ecu_hardware_id;
        conf.secondary_type = Uptane::SecondaryType::kIpUptane;
        secondaries.push_back(conf);
      }
    } catch (const deserialization_error ex) {
      LOG_ERROR << ex.what();
    }
    auto now = std::chrono::system_clock::now();
    int left_seconds =
        config_.ipdiscovery_wait_seconds - std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    if (left_seconds <= 0) {
      break;
    }
    tv.tv_sec = left_seconds;
    setsockopt(*socket_hdl, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
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
  struct sockaddr_in sendaddr {};
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons(config_.ipdiscovery_port);
  sendaddr.sin_addr.s_addr = inet_addr(config_.ipdiscovery_host.c_str());

  asn1::Serializer ser;
  ser << asn1::seq;
  ser << asn1::implicit<kAsn1Integer>(AKT_DISCOVERY_REQ);
  ser << asn1::endseq;
  int numbytes = sendto(*socket_hdl, ser.getResult().c_str(), ser.getResult().size(), 0, (struct sockaddr *)&sendaddr,
                        sizeof sendaddr);
  if (numbytes == -1) {
    LOG_ERROR << "Could not send discovery request: " << std::strerror(errno);
    return false;
  }
  return true;
}
