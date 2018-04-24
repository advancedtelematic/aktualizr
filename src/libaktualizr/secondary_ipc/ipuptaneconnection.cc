#include "secondary_ipc/ipuptaneconnection.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "logging/logging.h"
#include "socket_activation/socket_activation.h"
#include "utilities/utils.h"

void IpUptaneConnection::open_socket() {
  if (socket_activation::listen_fds(0) >= 1) {
    LOG_INFO << "Using socket activation for main service";
    socket_hdl_ = SocketHandle(new int(socket_activation::listen_fds_start));
    return;
  }

  LOG_INFO << "Received " << socket_activation::listen_fds(0)
           << " sockets, not using socket activation for main service";

  // manual socket creation
  int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    throw std::runtime_error("socket creation failed");
  }
  SocketHandle hdl(new int(socket_fd));
  sockaddr_in6 sa{};

  memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(in_port_);
  sa.sin6_addr = in_addr_;

  int v6only = 0;
  if (setsockopt(*hdl, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(IPV6_V6ONLY)");
  }

  int reuseaddr = 1;
  if (setsockopt(*hdl, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_REUSEADDR)");
  }

  if (bind(*hdl, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0) {
    throw std::system_error(errno, std::system_category(), "bind");
  }

  if (listen(*hdl, SOMAXCONN) < 0) {
    throw std::system_error(errno, std::system_category(), "listen");
  }

  socket_hdl_ = std::move(hdl);

  std::string addr_string = Utils::ipDisplayName(*(reinterpret_cast<sockaddr_storage *>(&sa)));
  LOG_INFO << "Listening on " << addr_string << ":" << listening_port();
}

IpUptaneConnection::IpUptaneConnection(in_port_t in_port, struct in6_addr in_addr)
    : in_port_(in_port), in_addr_(in_addr) {
  open_socket();

  in_thread_ = std::thread([this]() {
    while (true) {
      std::unique_ptr<sockaddr_storage> peer_sa(new sockaddr_storage);
      socklen_t other_sasize = sizeof(*peer_sa);
      int con_fd;

      if ((con_fd = accept(*socket_hdl_, reinterpret_cast<sockaddr *>(peer_sa.get()), &other_sasize)) == -1) {
        break;
      }

      // One thread per connection
      std::thread(&IpUptaneConnection::handle_connection_msgs, this, SocketHandle(new int(con_fd)), std::move(peer_sa))
          .detach();
    }

    in_channel_.close();
  });

  out_thread_ = std::thread([this]() {
    std::shared_ptr<SecondaryPacket> pkt;
    while (out_channel_ >> pkt) {
      socklen_t addr_len;
      int socket_fd;
      if (pkt->peer_addr.ss_family == AF_INET) {
        std::runtime_error("IPv4 is not supported");
      };
      addr_len = sizeof(sockaddr_in6);
      socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
      if (socket_fd < 0) {
        throw std::system_error(errno, std::system_category(), "socket");
      }

      SocketHandle hdl(new int(socket_fd));
      sockaddr_in6 sa{};

      memset(&sa, 0, sizeof(sa));
      sa.sin6_family = AF_INET6;
      sa.sin6_port = 0;
      sa.sin6_addr = in_addr_;

      if (bind(*hdl, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0) {
        throw std::system_error(errno, std::system_category(), "bind");
      }

      LOG_INFO << "Outstanding connection to" << Utils::ipDisplayName(pkt->peer_addr) << ":"
               << Utils::ipPort(pkt->peer_addr);
      if (connect(*hdl, reinterpret_cast<struct sockaddr *>(&pkt->peer_addr), addr_len) < 0) {
        LOG_ERROR << "connect: " << std::strerror(errno);
        continue;
      }

      asn1::Serializer asn1_ser;
      asn1_ser << *pkt->msg;
      const std::string &result = asn1_ser.getResult();
      const char *data = result.c_str();
      size_t len = result.length();
      size_t pos = 0;

      while (len > 0) {
        ssize_t written = write(*hdl, data + pos, len);
        if (written < 0) {
          LOG_ERROR << "write: " << std::strerror(errno);
          shutdown(*hdl, SHUT_RDWR);
          break;
        }
        len -= written;
        pos += written;
      }
      shutdown(*hdl, SHUT_RDWR);
    }
  });
}

IpUptaneConnection::~IpUptaneConnection() {
  try {
    stop();
  } catch (...) {
    // ignore exceptions
  }
}

void IpUptaneConnection::stop() {
  shutdown(*socket_hdl_, SHUT_RDWR);
  in_thread_.join();
  out_channel_.close();
  out_thread_.join();
}

void IpUptaneConnection::handle_connection_msgs(SocketHandle con, std::unique_ptr<sockaddr_storage> addr) {
  std::string peer_name = Utils::ipDisplayName(*addr);
  LOG_INFO << "Incoming connection from " << peer_name << ":" << Utils::ipPort(*addr);
  std::string message_content;
  while (true) {
    uint8_t c;
    if (recv(*con, &c, 1, 0) != 1) {
      break;
    }
    message_content.push_back(c);
  }
  LOG_TRACE << "Received message: " << Utils::toBase64(message_content);
  asn1::Deserializer asn1_stream(message_content);
  std::unique_ptr<SecondaryMessage> mes;
  try {
    asn1_stream >> mes;
  } catch (deserialization_error &) {
    LOG_ERROR << "Failed to parse message: " << Utils::toBase64(message_content);
    return;
  }
  std::shared_ptr<SecondaryPacket> pkt{new SecondaryPacket{*addr, std::move(mes)}};

  in_channel_ << pkt;
  LOG_INFO << "Connection closed with " << peer_name;
}

in_port_t IpUptaneConnection::listening_port() const {
  sockaddr_storage ss{};
  socklen_t len = sizeof(ss);
  in_port_t p;
  if (getsockname(*socket_hdl_, reinterpret_cast<sockaddr *>(&ss), &len) < 0) {
    return -1;
  }

  if (ss.ss_family == AF_INET) {
    auto *sa = reinterpret_cast<sockaddr_in *>(&ss);
    p = sa->sin_port;
  } else if (ss.ss_family == AF_INET6) {
    auto *sa = reinterpret_cast<sockaddr_in6 *>(&ss);
    p = sa->sin6_port;
  } else {
    return -1;
  }

  return ntohs(p);
}

sockaddr_storage IpUptaneConnection::listening_address() const {
  sockaddr_storage ss{};
  socklen_t len = sizeof(ss);
  if (getsockname(*socket_hdl_, reinterpret_cast<sockaddr *>(&ss), &len) < 0) {
    throw std::system_error(errno, std::system_category(), "getsockname");
  }

  return ss;
}
