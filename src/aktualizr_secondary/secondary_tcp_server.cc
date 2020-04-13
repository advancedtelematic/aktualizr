#include "secondary_tcp_server.h"

#include <netinet/tcp.h>

#include "AKInstallationResultCode.h"
#include "AKIpUptaneMes.h"
#include "asn1/asn1_message.h"
#include "logging/logging.h"
#include "msg_dispatcher.h"
#include "utilities/dequeue_buffer.h"

SecondaryTcpServer::SecondaryTcpServer(MsgDispatcher &msg_dispatcher, const std::string &primary_ip,
                                       in_port_t primary_port, in_port_t port, bool reboot_after_install)
    : msg_dispatcher_(msg_dispatcher),
      listen_socket_(port),
      keep_running_(true),
      reboot_after_install_(reboot_after_install),
      is_running_(false) {
  if (primary_ip.empty()) {
    return;
  }

  ConnectionSocket conn_socket(primary_ip, primary_port, listen_socket_.port());
  if (conn_socket.connect() == 0) {
    LOG_INFO << "Connected to Primary, sending info about this Secondary.";
    HandleOneConnection(*conn_socket);
  } else {
    LOG_INFO << "Failed to connect to Primary.";
  }
}

void SecondaryTcpServer::run() {
  if (listen(*listen_socket_, SOMAXCONN) < 0) {
    throw std::system_error(errno, std::system_category(), "listen");
  }
  LOG_INFO << "Secondary TCP server listening on " << listen_socket_.toString();

  {
    std::unique_lock<std::mutex> lock(running_condition_mutex_);
    is_running_ = true;
    running_condition_.notify_all();
  }

  while (keep_running_.load()) {
    sockaddr_storage peer_sa{};
    socklen_t peer_sa_size = sizeof(sockaddr_storage);

    LOG_DEBUG << "Waiting for connection from Primary...";
    int con_fd = accept(*listen_socket_, reinterpret_cast<sockaddr *>(&peer_sa), &peer_sa_size);
    if (con_fd == -1) {
      // Accept can fail if a client closes connection/client socket before a TCP handshake completes or
      // a network connection goes down in the middle of a TCP handshake procedure. At first glance it looks like
      // we can just continue listening/accepting new connections in such cases instead of exiting from the server loop
      // which leads to exiting of the overall daemon process.
      // But, accept() failure, potentially can be caused by some incorrect state of the listening socket
      // which means that it will keep returning error, so, exiting from the daemon process and letting
      // systemd to restart it looks like the most reliable solution that covers all edge cases.
      LOG_INFO << "Socket accept failed, aborting.";
      break;
    }

    LOG_DEBUG << "Primary connected.";
    auto continue_running = HandleOneConnection(*Socket(con_fd));
    if (!continue_running) {
      keep_running_.store(false);
    }
    LOG_DEBUG << "Primary disconnected.";
  }

  {
    std::unique_lock<std::mutex> lock(running_condition_mutex_);
    is_running_ = false;
    running_condition_.notify_all();
  }

  LOG_INFO << "Secondary TCP server exiting.";
}

void SecondaryTcpServer::stop() {
  LOG_DEBUG << "Stopping Secondary TCP server...";
  keep_running_.store(false);
  // unblock accept
  ConnectionSocket("localhost", listen_socket_.port()).connect();
}

in_port_t SecondaryTcpServer::port() const { return listen_socket_.port(); }
SecondaryTcpServer::ExitReason SecondaryTcpServer::exit_reason() const { return exit_reason_; }

static bool sendResponseMessage(int socket_fd, Asn1Message::Ptr &resp_msg);

bool SecondaryTcpServer::HandleOneConnection(int socket) {
  // Outside the message loop, because one recv() may have parts of 2 messages
  // Note that one recv() call returning 2+ messages doesn't work at the
  // moment. This shouldn't be a problem until we have messages that aren't
  // strictly request/response
  DequeueBuffer buffer;
  bool keep_running_server = true;
  bool keep_running_current_session = true;

  while (keep_running_current_session) {  // Keep reading until we get an error
    // Read an incomming message
    AKIpUptaneMes_t *m = nullptr;
    asn_dec_rval_t res;
    asn_codec_ctx_s context{};
    ssize_t received;

    do {
      received = recv(socket, buffer.Tail(), buffer.TailSpace(), 0);
      buffer.HaveEnqueued(static_cast<size_t>(received));
      res = ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void **>(&m), buffer.Head(), buffer.Size());
      buffer.Consume(res.consumed);
    } while (res.code == RC_WMORE && received > 0);
    // Note that ber_decode allocates *m even on failure, so this must always be done
    Asn1Message::Ptr request_msg = Asn1Message::FromRaw(&m);

    if (received <= 0) {
      LOG_DEBUG << "Primary has closed a connection socket";
      break;
    }

    if (res.code != RC_OK) {
      LOG_ERROR << "Failed to receive and/or decode a message from Primary";
      break;
    }

    LOG_DEBUG << "Received message from Primary, try to decode it...";
    Asn1Message::Ptr response_msg = Asn1Message::Empty();
    MsgDispatcher::HandleStatusCode handle_status_code = msg_dispatcher_.handleMsg(request_msg, response_msg);

    switch (handle_status_code) {
      case MsgDispatcher::HandleStatusCode::kRebootRequired: {
        exit_reason_ = ExitReason::kRebootNeeded;
        keep_running_current_session = sendResponseMessage(socket, response_msg);
        if (reboot_after_install_) {
          keep_running_server = keep_running_current_session = false;
        }
        break;
      }
      case MsgDispatcher::HandleStatusCode::kOk: {
        keep_running_current_session = sendResponseMessage(socket, response_msg);
        break;
      }
      case MsgDispatcher::HandleStatusCode::kUnkownMsg:
      default: {
        // TODO: consider sending NOT_SUPPORTED/Unknown message and closing connection socket
        keep_running_current_session = false;
        LOG_INFO << "Unknown message received from Primary!";
      }
    }  // switch

  }  // Go back round and read another message

  return keep_running_server;
  // Parse error => Shutdown the socket
  // write error => Shutdown the socket
  // Timeout on write => shutdown
}

void SecondaryTcpServer::wait_until_running(int timeout) {
  std::unique_lock<std::mutex> lock(running_condition_mutex_);
  running_condition_.wait_for(lock, std::chrono::seconds(timeout), [&] { return is_running_; });
}

bool sendResponseMessage(int socket_fd, Asn1Message::Ptr &resp_msg) {
  LOG_DEBUG << "Encoding and sending response message";

  int optval = 0;
  setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
  asn_enc_rval_t encode_result = der_encode(&asn_DEF_AKIpUptaneMes, &resp_msg->msg_, Asn1SocketWriteCallback,
                                            reinterpret_cast<void *>(&socket_fd));
  if (encode_result.encoded == -1) {
    LOG_ERROR << "Failed to encode a response message";
    return false;  // write error
  }
  optval = 1;
  setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));

  return true;
}
