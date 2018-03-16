#include "aktualizr_secondary.h"

#include "aktualizr_secondary_discovery.h"

#include <future>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "logging.h"
#include "socket_activation.h"
#include "utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig &config) : config_(config) { open_socket(); }

void AktualizrSecondary::open_socket() {
  if (socket_activation::listen_fds(0) >= 1) {
    LOG_INFO << "Using socket activation";
    socket_hdl_ = SocketHandle(new int(socket_activation::listen_fds_start));
    return;
  }

  // manual socket creation
  int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
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
  sa.sin6_port = htons(config_.network.port);
  sa.sin6_addr = IN6ADDR_ANY_INIT;

  if (bind(*hdl, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0) {
    LOG_ERROR << "bind failed: " << std::strerror(errno) << "\n";
    throw std::runtime_error("bind failed");
  }

  if (listen(*hdl, SOMAXCONN) < 0) {
    LOG_ERROR << "listen failed: " << std::strerror(errno) << "\n";
    throw std::runtime_error("listen failed");
  }

  socket_hdl_ = std::move(hdl);
}

void AktualizrSecondary::run() {
  // discovery
  AktualizrSecondaryDiscovery discovery(config_.network);
  std::thread discovery_thread = std::thread(&AktualizrSecondaryDiscovery::run, &discovery);

  // listen for TCP connections
  std::thread tcp_thread = std::thread([this]() {
    LOG_INFO << "Listening on port " << listening_port();

    while (true) {
      std::unique_ptr<sockaddr_storage> peer_sa(new sockaddr_storage);
      socklen_t other_sasize = sizeof(*peer_sa);
      int con_fd;

      if ((con_fd = accept(*socket_hdl_, reinterpret_cast<sockaddr *>(peer_sa.get()), &other_sasize)) == -1) {
        break;
      }

      // One thread per connection
      std::thread(&AktualizrSecondary::handle_connection_msgs, this, SocketHandle(new int(con_fd)), std::move(peer_sa))
          .detach();
    }

    channel_.close();
  });

  // listen for messages
  std::shared_ptr<SecondaryPacket> pkt;
  while (channel_ >> pkt) {
    // TODO: process message
  }

  discovery.stop();

  tcp_thread.join();
  discovery_thread.join();
}

int AktualizrSecondary::listening_port() const { return Utils::ipPort(Utils::ipGetSockaddr(*socket_hdl_)); }
void AktualizrSecondary::stop() { shutdown(*socket_hdl_, SHUT_RDWR); }

void AktualizrSecondary::handle_connection_msgs(SocketHandle con, std::unique_ptr<sockaddr_storage> addr) {
  std::string peer_name = Utils::ipDisplayName(*addr);
  LOG_INFO << "Opening connection with " << peer_name;
  std::string message_content;
  while (true) {
    uint8_t c;
    if (recv(*con, &c, 1, 0) != 1) {
      break;
    }
    message_content.push_back(c);
    asn1::Deserializer asn1_stream(message_content);
    std::unique_ptr<SecondaryMessage> mes;
    try {
      asn1_stream >> mes;
    } catch (deserialization_error) {
      LOG_ERROR << "Unexpected message format";
      continue;
    }
    std::unique_ptr<SecondaryPacket> pkt{new SecondaryPacket{*addr, std::move(mes)}};

    channel_ << std::move(pkt);
  }
  LOG_INFO << "Connection closed with " << peer_name;
}
std::string AktualizrSecondary::getSerialResp() {
  LOG_ERROR << "getSerialResp is not implemented yet";
  return "";
}

std::string AktualizrSecondary::getHwIdResp() {
  LOG_ERROR << "getHwIdResp is not implemented yet";
  return "";
}

std::pair<KeyType, std::string> AktualizrSecondary::getPublicKeyResp() {
  LOG_ERROR << "getPublicKeyResp is not implemented yet";
  return std::make_pair(kUnknownKey, "");
}

Json::Value AktualizrSecondary::getManifestResp() {
  LOG_ERROR << "getManifestResp is not implemented yet";
  return Json::Value();
}

bool AktualizrSecondary::putMetadataResp(const Uptane::MetaPack &meta_pack) {
  (void)meta_pack;
  LOG_ERROR << "putMedatadatResp is not implemented yet";
  return false;
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) {
  (void)director;
  LOG_ERROR << "getRootVersionResp is not implemented yet";
  return -1;
}

bool AktualizrSecondary::putRootResp(Uptane::Root root, bool director) {
  (void)root;
  (void)director;
  LOG_ERROR << "putRootResp is not implemented yet";
  return false;
}

bool AktualizrSecondary::sendFirmwareResp(const std::string &firmware) {
  (void)firmware;
  LOG_ERROR << "sendFirmwareResp is not implemented yet";
  return false;
}
