#include "aktualizr_secondary_discovery.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "asn1-cerstream.h"
#include "ipsecondarydiscovery.h"
#include "logging.h"
#include "socket_activation.h"
#include "utils.h"

AktualizrSecondaryDiscovery::AktualizrSecondaryDiscovery(const AktualizrSecondaryNetConfig &config) : config_(config) {
  if (!config_.discovery) {
    return;
  }

  open_socket();
}

void AktualizrSecondaryDiscovery::open_socket() {
  if (socket_activation::listen_fds(0) >= 2) {
    LOG_INFO << "Using socket activation for discovery service";
    socket_hdl_ = SocketHandle(new int(socket_activation::listen_fds_start + 1));
    return;
  }

  int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    throw std::runtime_error("socket creation failed");
  }
  SocketHandle hdl(new int(socket_fd));

  int v6only = 0;
  if (setsockopt(*hdl, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
    throw std::runtime_error("setsockopt(IPV6_V6ONLY) failed");
  }

  int reuseaddr = 1;
  if (setsockopt(*hdl, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
    throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
  }

  sockaddr_in6 sa{};

  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(config_.discovery_port);
  sa.sin6_addr = IN6ADDR_ANY_INIT;

  if (bind(*hdl, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0) {
    throw std::runtime_error("bind failed");
  }

  socket_hdl_ = std::move(hdl);
}

void AktualizrSecondaryDiscovery::run() {
  if (!config_.discovery) {
    return;
  }

  LOG_INFO << "Discovery listening on " << listening_port();

  while (true) {
    char buf[2000];
    struct sockaddr_storage peer {};
    socklen_t sa_size = sizeof(peer);
    int received = recvfrom(*socket_hdl_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&peer), &sa_size);
    if (received == 0) {
      // socket closed
      break;
    }
    if (received < 0) {
      // error
      LOG_ERROR << "recv error: " << std::strerror(errno);
      break;
    }

    std::string msg(buf, received);
    try {
      asn1::Deserializer des(msg);
      int type = 0;
      des >> asn1::seq >> asn1::implicit<kAsn1Integer>(type) >> asn1::restseq;

      if (type == AKT_DISCOVERY_REQ) {
        LOG_INFO << "Got discovery request from " << Utils::ipDisplayName(peer);
        std::string smsg;
        asn1::Serializer ser;
        // FIXME: real serial and hardware id
        std::string hwid = "hardware";
        std::string serialid = "serial";
        ser << asn1::seq << asn1::implicit<kAsn1Integer>(AKT_DISCOVERY_RESP)
            << asn1::implicit<kAsn1Utf8String>(serialid) << asn1::implicit<kAsn1Utf8String>(hwid) << asn1::endseq;

        if (sendto(*socket_hdl_, ser.getResult().c_str(), ser.getResult().size(), 0,
                   reinterpret_cast<sockaddr *>(&peer), sa_size) < 0) {
          LOG_ERROR << "send failed: " << std::strerror(errno);
        }
      }
    } catch (const deserialization_error &exc) {
      LOG_ERROR << exc.what() << " from " << Utils::ipDisplayName(peer);
    }
  }
}

int AktualizrSecondaryDiscovery::listening_port() const { return Utils::ipPort(Utils::ipGetSockaddr(*socket_hdl_)); }

void AktualizrSecondaryDiscovery::stop() {
  if (!config_.discovery) {
    return;
  }

  shutdown(*socket_hdl_, SHUT_RDWR);
}
