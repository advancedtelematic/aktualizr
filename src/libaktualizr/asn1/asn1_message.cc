#include "asn1/asn1_message.h"
#include "logging/logging.h"
#include "utilities/dequeue_buffer.h"
#include "utilities/sockaddr_io.h"
#include "utilities/utils.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

int Asn1StringAppendCallback(const void* buffer, size_t size, void* priv) {
  auto out_str = static_cast<std::string*>(priv);
  out_str->append(std::string(static_cast<const char*>(buffer), size));
  return 0;
}

#ifdef MSG_NOSIGNAL
constexpr int flags_no_signal = MSG_NOSIGNAL;
#else
constexpr int flags_no_signal = 0;  // OS X
#endif

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
    ssize_t written = send(*sock, b + pos, len, flags_no_signal);
    if (written < 0) {
      LOG_ERROR << "write: " << std::strerror(errno);
      return 1;
    }
    len -= written;
    pos += written;
  }
  return 0;
}

std::string ToString(const OCTET_STRING_t& octet_str) {
  return std::string(reinterpret_cast<const char*>(octet_str.buf), octet_str.size);
}

void SetString(OCTET_STRING_t* dest, const std::string& str) { OCTET_STRING_fromBuf(dest, str.c_str(), str.size()); }

Asn1Message::Ptr Asn1Rpc(const Asn1Message::Ptr& tx, const struct sockaddr_storage& client) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
  SocketHandle hdl(new int(socket_fd));
  if (connect(*hdl, reinterpret_cast<const struct sockaddr*>(&client), sizeof(sockaddr_in6)) < 0) {
    LOG_ERROR << "connect to " << client << " failed:" << std::strerror(errno);
    return Asn1Message::Empty();
  }
  der_encode(&asn_DEF_AKIpUptaneMes, &tx->msg_, Asn1SocketWriteCallback, hdl.get());

  // Bounce TCP_NODELAY to flush the TCP send buffer
  int no_delay = 1;
  setsockopt(*hdl, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));
  no_delay = 0;
  setsockopt(*hdl, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));

  AKIpUptaneMes_t* m = nullptr;
  asn_dec_rval_t res;
  asn_codec_ctx_s context{};
  DequeueBuffer buffer;
  size_t received;
  do {
    received = recv(*hdl, buffer.Tail(), buffer.TailSpace(), 0);
    LOG_TRACE << "Asn1Rpc read " << Utils::toBase64(std::string(buffer.Tail(), received));
    buffer.HaveEnqueued(received);
    res = ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void**>(&m), buffer.Head(), buffer.Size());
    buffer.Consume(res.consumed);
  } while (res.code == RC_WMORE && received > 0);
  // Note that ber_decode allocates *m even on failure, so this must always be done
  Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

  if (res.code != RC_OK) {
    LOG_ERROR << "Asn1Rpc decoding failed";
    msg->present(AKIpUptaneMes_PR_NOTHING);
  }
  shutdown(*hdl, SHUT_RDWR);

  return msg;
}
