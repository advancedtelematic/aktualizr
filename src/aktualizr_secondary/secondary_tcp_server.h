#ifndef AKTUALIZR_SECONDARY_TCP_SERVER_H_
#define AKTUALIZR_SECONDARY_TCP_SERVER_H_

#include "utilities/utils.h"

#include <atomic>

namespace Uptane {
class SecondaryInterface;
}  // namespace Uptane
/**
 * Listens on a socket, decodes calls (ASN.1) and forwards them to an Uptane Secondary
 * implementation
 */
class SecondaryTcpServer {
 public:
  enum class ExitReason {
    kNotApplicable,
    kRebootNeeded,
    kUnkown,
  };

  SecondaryTcpServer(Uptane::SecondaryInterface& secondary, const std::string& primary_ip, in_port_t primary_port,
                     in_port_t port = 0, bool reboot_after_install = false);

  SecondaryTcpServer(const SecondaryTcpServer&) = delete;
  SecondaryTcpServer& operator=(const SecondaryTcpServer&) = delete;

 public:
  /**
   * Accept connections on the socket, decode requests and respond using the secondary implementation
   */
  void run();
  void stop();

  in_port_t port() const;
  ExitReason exit_reason() const;

 private:
  bool HandleOneConnection(int socket);

 private:
  Uptane::SecondaryInterface& impl_;
  ListenSocket listen_socket_;
  std::atomic<bool> keep_running_;
  bool reboot_after_install_;
  ExitReason exit_reason_{ExitReason::kNotApplicable};
};

#endif  // AKTUALIZR_SECONDARY_TCP_SERVER_H_
