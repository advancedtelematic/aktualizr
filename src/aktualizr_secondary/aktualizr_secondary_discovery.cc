#include "aktualizr_secondary_discovery.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "asn1/asn1-cerstream.h"
#include "ipsecondarydiscovery.h"
#include "logging/logging.h"
#include "socket_activation/socket_activation.h"
#include "utilities/utils.h"

AktualizrSecondaryDiscovery::AktualizrSecondaryDiscovery(const AktualizrSecondaryNetConfig &config,
                                                         AktualizrSecondary &akt_secondary)
    : config_(config), akt_secondary_(akt_secondary) {
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

  LOG_TRACE << "Received " << socket_activation::listen_fds(0)
            << " sockets, not using socket activation for discovery service";

  int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
  SocketHandle hdl(new int(socket_fd));

  int v6only = 0;
  if (setsockopt(*hdl, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(IPV6_V6ONLY)");
  }

  int reuseaddr = 1;
  if (setsockopt(*hdl, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_REUSEADDR)");
  }

  sockaddr_in6 sa{};

  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(config_.discovery_port);
  sa.sin6_addr = IN6ADDR_ANY_INIT;

  if (bind(*hdl, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0) {
    throw std::system_error(errno, std::system_category(), "bind");
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
      int32_t primary_port;
      asn1::Deserializer des(msg);
      des >> asn1::expl(AKT_DISCOVERY_REQ) >> asn1::seq >> asn1::implicit<kAsn1Integer>(primary_port) >>
          asn1::restseq >> asn1::endexpl;

      LOG_TRACE << "Got discovery request from " << Utils::ipDisplayName(peer) << ":" << primary_port;
      std::string smsg;
      asn1::Serializer ser;
      std::string hwid = akt_secondary_.getHwIdResp();
      std::string serialid = akt_secondary_.getSerialResp();
      int32_t port_32 = config_.port;
      ser << asn1::expl(AKT_DISCOVERY_RESP) << asn1::seq << asn1::implicit<kAsn1Utf8String>(serialid)
          << asn1::implicit<kAsn1Utf8String>(hwid) << asn1::implicit<kAsn1Integer>(port_32) << asn1::endseq
          << asn1::endexpl;

      if (sendto(*socket_hdl_, ser.getResult().c_str(), ser.getResult().size(), 0, reinterpret_cast<sockaddr *>(&peer),
                 sa_size) < 0) {
        LOG_ERROR << "send failed: " << std::strerror(errno);
      }
      akt_secondary_.addPrimary(peer, primary_port);
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
