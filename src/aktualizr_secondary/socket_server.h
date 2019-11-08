#ifndef AKTUALIZR_SECONDARY_SOCKET_SERVER_H_
#define AKTUALIZR_SECONDARY_SOCKET_SERVER_H_

#include "uptane/secondaryinterface.h"
#include "utilities/utils.h"

/**
 * Listens on a socket, decodes calls and forwards them to an Uptane Secondary
 * implementation
 */
class SocketServer {
 public:
  SocketServer(std::shared_ptr<Uptane::SecondaryInterface> implementation, SocketHandle socket)
      : impl_(std::move(implementation)), socket_(std::move(socket)) {}

  /**
   * Accept connections on the socket, decode requests and respond using the
   * wrapped secondary
   */
  void Run();

  void HandleOneConnection(int socket);

 private:
  std::shared_ptr<Uptane::SecondaryInterface> impl_;
  SocketHandle socket_;
};

/**
 * Create and return a server socket on the given port number
 */
SocketHandle SocketFromPort(in_port_t port);

#endif  // AKTUALIZR_SECONDARY_SOCKET_SERVER_H_
