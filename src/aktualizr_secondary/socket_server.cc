#include "socket_server.h"

#include <future>

#include "AKIpUptaneMes.h"
#include "asn1/asn1_message.h"
#include "logging/logging.h"
#include "utilities/dequeue_buffer.h"
#include "utilities/sockaddr_io.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

void SocketServer::Run() {
  if (listen(*socket_, SOMAXCONN) < 0) {
    throw std::system_error(errno, std::system_category(), "listen");
  }
  LOG_INFO << "Listening on " << Utils::ipGetSockaddr(*socket_);

  while (true) {
    int con_fd;
    sockaddr_storage peer_sa{};
    socklen_t peer_sa_size = sizeof(sockaddr_storage);

    LOG_DEBUG << "Waiting for connection from client...";
    if ((con_fd = accept(*socket_, reinterpret_cast<sockaddr *>(&peer_sa), &peer_sa_size)) == -1) {
      LOG_INFO << "Socket accept failed. aborting";
      break;
    }
    LOG_DEBUG << "Connected...";
    HandleOneConnection(con_fd);
    LOG_DEBUG << "Client disconnected";
  }
}

void SocketServer::HandleOneConnection(int socket) {
  // Outside the message loop, because one recv() may have parts of 2 messages
  // Note that one recv() call returning 2+ messages doesn't work at the
  // moment. This shouldn't be a problem until we have messages that aren't
  // strictly request/response
  DequeueBuffer buffer;

  while (true) {  // Keep reading until we get an error
    // Read an incomming message
    AKIpUptaneMes_t *m = nullptr;
    asn_dec_rval_t res;
    asn_codec_ctx_s context{};
    ssize_t received;
    do {
      received = recv(socket, buffer.Tail(), buffer.TailSpace(), 0);
      LOG_TRACE << "Got " << received << " bytes "
                << Utils::toBase64(std::string(buffer.Tail(), static_cast<size_t>(received)));
      buffer.HaveEnqueued(static_cast<size_t>(received));
      res = ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void **>(&m), buffer.Head(), buffer.Size());
      buffer.Consume(res.consumed);
    } while (res.code == RC_WMORE && received > 0);
    // Note that ber_decode allocates *m even on failure, so this must always be done
    Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

    if (res.code != RC_OK) {
      return;  // Either an error or the client closed the socket
    }

    // Figure out what to do with the message
    Asn1Message::Ptr resp = Asn1Message::Empty();
    switch (msg->present()) {
      case AKIpUptaneMes_PR_getInfoReq: {
        Uptane::EcuSerial serial = impl_->getSerial();
        Uptane::HardwareIdentifier hw_id = impl_->getHwId();
        PublicKey pk = impl_->getPublicKey();
        resp->present(AKIpUptaneMes_PR_getInfoResp);
        auto r = resp->getInfoResp();
        SetString(&r->ecuSerial, serial.ToString());
        SetString(&r->hwId, hw_id.ToString());
        r->keyType = static_cast<AKIpUptaneKeyType_t>(pk.Type());
        SetString(&r->key, pk.Value());
      } break;
      case AKIpUptaneMes_PR_manifestReq: {
        std::string manifest = Utils::jsonToStr(impl_->getManifest());
        resp->present(AKIpUptaneMes_PR_manifestResp);
        auto r = resp->manifestResp();
        r->manifest.present = manifest_PR_json;
        SetString(&r->manifest.choice.json, manifest);  // NOLINT
      } break;
      case AKIpUptaneMes_PR_putMetaReq: {
        auto md = msg->putMetaReq();
        Uptane::RawMetaPack meta_pack;
        if (md->image.present == image_PR_json) {
          meta_pack.image_root = ToString(md->image.choice.json.root);            // NOLINT
          meta_pack.image_targets = ToString(md->image.choice.json.targets);      // NOLINT
          meta_pack.image_snapshot = ToString(md->image.choice.json.snapshot);    // NOLINT
          meta_pack.image_timestamp = ToString(md->image.choice.json.timestamp);  // NOLINT
        } else {
          LOG_WARNING << "Images metadata in unknown format:" << md->image.present;
        }

        if (md->director.present == director_PR_json) {
          meta_pack.director_root = ToString(md->director.choice.json.root);        // NOLINT
          meta_pack.director_targets = ToString(md->director.choice.json.targets);  // NOLINT
        } else {
          LOG_WARNING << "Director metadata in unknown format:" << md->director.present;
        }
        bool ok;
        try {
          ok = impl_->putMetadata(meta_pack);
        } catch (Uptane::SecurityException &e) {
          LOG_WARNING << "Rejected metadata push because of security failure" << e.what();
          ok = false;
        }
        resp->present(AKIpUptaneMes_PR_putMetaResp);
        auto r = resp->putMetaResp();
        r->result = ok ? AKInstallationResult_success : AKInstallationResult_failure;
      } break;
      case AKIpUptaneMes_PR_sendFirmwareReq: {
        auto fw = msg->sendFirmwareReq();
        auto fw_data = std::make_shared<std::string>(ToString(fw->firmware));
        auto fut = std::async(std::launch::async, &Uptane::SecondaryInterface::sendFirmware, impl_, fw_data);
        resp->present(AKIpUptaneMes_PR_sendFirmwareResp);
        auto r = resp->sendFirmwareResp();
        r->result = fut.get() ? AKInstallationResult_success : AKInstallationResult_failure;
      } break;
      default:
        LOG_ERROR << "Unrecognised message type:" << msg->present();
        return;
    }

    // Send the response
    if (resp->present() != AKIpUptaneMes_PR_NOTHING) {
      int optval = 0;
      setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
      asn_enc_rval_t encode_result =
          der_encode(&asn_DEF_AKIpUptaneMes, &resp->msg_, Asn1SocketWriteCallback, reinterpret_cast<void *>(&socket));
      if (encode_result.encoded == -1) {
        return;  // write error
      }
      optval = 1;
      setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
    } else {
      LOG_DEBUG << "Not sending a response to message " << msg->present();
    }
  }  // Go back round and read another message

  // Parse error => Shutdown the socket
  // write error => Shutdown the socket
  // Timeout on write => shutdown
}

SocketHandle SocketFromPort(in_port_t port) {
  // manual socket creation
  int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    throw std::runtime_error("socket creation failed");
  }
  SocketHandle hdl(new int(socket_fd));
  sockaddr_in6 sa{};

  memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(port);
  sa.sin6_addr = IN6ADDR_ANY_INIT;

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

  return hdl;
}
