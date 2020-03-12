#include "asn1_message.h"
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "logging/logging.h"
#include "utilities/dequeue_buffer.h"
#include "utilities/utils.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include <sys/socket.h>
#include <sys/types.h>

int Asn1StringAppendCallback(const void* buffer, size_t size, void* priv) {
  auto out_str = static_cast<std::string*>(priv);
  out_str->append(std::string(static_cast<const char*>(buffer), size));
  return 0;
}

/**
 * Adaptor to write output of der_encode to a socket
 * priv is a SocketHandle
 */
int Asn1SocketWriteCallback(const void* buffer, size_t size, void* priv) {
  auto sock = reinterpret_cast<int*>(priv);  // NOLINT
  assert(sock != nullptr);
  assert(-1 < *sock);

  auto b = static_cast<const char*>(buffer);
  size_t len = size;
  size_t pos = 0;

  while (len > 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ssize_t written = send(*sock, b + pos, len, MSG_NOSIGNAL);
    if (written < 0) {
      LOG_ERROR << "write: " << std::strerror(errno);
      return 1;
    }
    len -= static_cast<size_t>(written);
    pos += static_cast<size_t>(written);
  }
  return 0;
}

std::string ToString(const OCTET_STRING_t& octet_str) {
  return std::string(reinterpret_cast<const char*>(octet_str.buf), static_cast<size_t>(octet_str.size));
}

void SetString(OCTET_STRING_t* dest, const std::string& str) {
  OCTET_STRING_fromBuf(dest, str.c_str(), static_cast<int>(str.size()));
}

Asn1Message::Ptr Asn1Rpc(const Asn1Message::Ptr& tx, int con_fd) {
  der_encode(&asn_DEF_AKIpUptaneMes, &tx->msg_, Asn1SocketWriteCallback, &con_fd);

  // Bounce TCP_NODELAY to flush the TCP send buffer
  int no_delay = 1;
  setsockopt(con_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));
  no_delay = 0;
  setsockopt(con_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));

  AKIpUptaneMes_t* m = nullptr;
  asn_dec_rval_t res;
  asn_codec_ctx_s context{};
  DequeueBuffer buffer;
  ssize_t received;
  do {
    received = recv(con_fd, buffer.Tail(), buffer.TailSpace(), 0);
    LOG_TRACE << "Asn1Rpc read " << Utils::toBase64(std::string(buffer.Tail(), static_cast<size_t>(received)));
    buffer.HaveEnqueued(static_cast<size_t>(received));
    res = ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void**>(&m), buffer.Head(), buffer.Size());
    buffer.Consume(res.consumed);
  } while (res.code == RC_WMORE && received > 0);
  // Note that ber_decode allocates *m even on failure, so this must always be done
  Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

  if (res.code != RC_OK) {
    LOG_ERROR << "Asn1Rpc decoding failed";
    msg->present(AKIpUptaneMes_PR_NOTHING);
  }

  return msg;
}

Asn1Message::Ptr Asn1Rpc(const Asn1Message::Ptr& tx, const std::pair<std::string, uint16_t>& addr,
                         uint16_t max_attempt_number) {
  ConnectionSocket connection(addr.first, addr.second);

  if (connection.connect(max_attempt_number) < 0) {
    LOG_ERROR << "Failed to connect to the secondary ( " << addr.first << ":" << addr.second
              << "): " << std::strerror(errno);
    return Asn1Message::Empty();
  }
  return Asn1Rpc(tx, *connection);
}
