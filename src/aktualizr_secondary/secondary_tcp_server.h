#ifndef AKTUALIZR_SECONDARY_TCP_SERVER_H_
#define AKTUALIZR_SECONDARY_TCP_SERVER_H_

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "utilities/utils.h"

class MsgDispatcher;

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

  SecondaryTcpServer(MsgDispatcher& msg_dispatcher, const std::string& primary_ip, in_port_t primary_port,
                     in_port_t port = 0, bool reboot_after_install = false);

  SecondaryTcpServer(const SecondaryTcpServer&) = delete;
  SecondaryTcpServer& operator=(const SecondaryTcpServer&) = delete;

 public:
  /**
   * Accept connections on the socket, decode requests and respond using the secondary implementation
   */
  void run();
  void stop();

  void wait_until_running(int timeout = 10);

  in_port_t port() const;
  ExitReason exit_reason() const;

 private:
  bool HandleOneConnection(int socket);

 private:
  MsgDispatcher& msg_dispatcher_;
  ListenSocket listen_socket_;
  std::atomic<bool> keep_running_;
  bool reboot_after_install_;
  ExitReason exit_reason_{ExitReason::kNotApplicable};

  bool is_running_;
  std::mutex running_condition_mutex_;
  std::condition_variable running_condition_;
};

#endif  // AKTUALIZR_SECONDARY_TCP_SERVER_H_
