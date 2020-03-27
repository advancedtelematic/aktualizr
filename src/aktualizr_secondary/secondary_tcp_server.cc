#include "secondary_tcp_server.h"

#include "AKIpUptaneMes.h"
#include "aktualizr_secondary.h"
#include "asn1/asn1_message.h"
#include "logging/logging.h"
#include "utilities/dequeue_buffer.h"

#include <netinet/tcp.h>

SecondaryTcpServer::SecondaryTcpServer(AktualizrSecondary &secondary, const std::string &primary_ip,
                                       in_port_t primary_port, in_port_t port, bool reboot_after_install)
    : impl_(secondary), listen_socket_(port), keep_running_(true), reboot_after_install_(reboot_after_install) {
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

  while (keep_running_.load()) {
    sockaddr_storage peer_sa{};
    socklen_t peer_sa_size = sizeof(sockaddr_storage);

    LOG_DEBUG << "Waiting for connection from Primary...";
    int con_fd = accept(*listen_socket_, reinterpret_cast<sockaddr *>(&peer_sa), &peer_sa_size);
    if (con_fd == -1) {
      LOG_INFO << "Socket accept failed, aborting.";
      break;
    }
    Socket con_socket(con_fd);
    LOG_DEBUG << "Connected to Primary.";
    bool continue_serving = HandleOneConnection(*con_socket);
    LOG_DEBUG << "Primary disconnected.";
    if (!continue_serving) {
      break;
    }
  }
  LOG_INFO << "Secondary TCP server exiting.";
}

void SecondaryTcpServer::stop() {
  keep_running_ = false;
  // unblock accept
  ConnectionSocket("localhost", listen_socket_.port()).connect();
}

in_port_t SecondaryTcpServer::port() const { return listen_socket_.port(); }
SecondaryTcpServer::ExitReason SecondaryTcpServer::exit_reason() const { return exit_reason_; }

bool SecondaryTcpServer::HandleOneConnection(int socket) {
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
    bool need_reboot = false;

    do {
      received = recv(socket, buffer.Tail(), buffer.TailSpace(), 0);
      buffer.HaveEnqueued(static_cast<size_t>(received));
      res = ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void **>(&m), buffer.Head(), buffer.Size());
      buffer.Consume(res.consumed);
    } while (res.code == RC_WMORE && received > 0);
    // Note that ber_decode allocates *m even on failure, so this must always be done
    Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

    if (res.code != RC_OK) {
      return true;  // Either an error or the client closed the socket
    }

    // Figure out what to do with the message
    Asn1Message::Ptr resp = Asn1Message::Empty();
    switch (msg->present()) {
      case AKIpUptaneMes_PR_getInfoReq: {
        Uptane::EcuSerial serial = impl_.getSerial();
        Uptane::HardwareIdentifier hw_id = impl_.getHwId();
        PublicKey pk = impl_.getPublicKey();
        resp->present(AKIpUptaneMes_PR_getInfoResp);
        auto r = resp->getInfoResp();
        SetString(&r->ecuSerial, serial.ToString());
        SetString(&r->hwId, hw_id.ToString());
        r->keyType = static_cast<AKIpUptaneKeyType_t>(pk.Type());
        SetString(&r->key, pk.Value());
      } break;
      case AKIpUptaneMes_PR_manifestReq: {
        std::string manifest = Utils::jsonToStr(impl_.getManifest());
        resp->present(AKIpUptaneMes_PR_manifestResp);
        auto r = resp->manifestResp();
        r->manifest.present = manifest_PR_json;
        SetString(&r->manifest.choice.json, manifest);  // NOLINT
        LOG_TRACE << "Manifest : \n" << manifest;
      } break;
      case AKIpUptaneMes_PR_putMetaReq: {
        auto md = msg->putMetaReq();
        Uptane::RawMetaPack meta_pack;

        if (md->director.present == director_PR_json) {
          meta_pack.director_root = ToString(md->director.choice.json.root);        // NOLINT
          meta_pack.director_targets = ToString(md->director.choice.json.targets);  // NOLINT
          LOG_DEBUG << "Received Director repo Root metadata:\n" << meta_pack.director_root;
          LOG_DEBUG << "Received Director repo Targets metadata:\n" << meta_pack.director_targets;
        } else {
          LOG_WARNING << "Director metadata in unknown format:" << md->director.present;
        }

        if (md->image.present == image_PR_json) {
          meta_pack.image_root = ToString(md->image.choice.json.root);            // NOLINT
          meta_pack.image_timestamp = ToString(md->image.choice.json.timestamp);  // NOLINT
          meta_pack.image_snapshot = ToString(md->image.choice.json.snapshot);    // NOLINT
          meta_pack.image_targets = ToString(md->image.choice.json.targets);      // NOLINT
          LOG_DEBUG << "Received Image repo Root metadata:\n" << meta_pack.image_root;
          LOG_DEBUG << "Received Image repo Timestamp metadata:\n" << meta_pack.image_timestamp;
          LOG_DEBUG << "Received Image repo Snapshot metadata:\n" << meta_pack.image_snapshot;
          LOG_DEBUG << "Received Image repo Targets metadata:\n" << meta_pack.image_targets;
        } else {
          LOG_WARNING << "Image repo metadata in unknown format:" << md->image.present;
        }

        bool ok;
        try {
          ok = impl_.putMetadata(meta_pack);
        } catch (Uptane::SecurityException &e) {
          LOG_WARNING << "Rejected metadata push because of security failure" << e.what();
          ok = false;
        }
        resp->present(AKIpUptaneMes_PR_putMetaResp);
        auto r = resp->putMetaResp();
        r->result = ok ? AKInstallationResult_success : AKInstallationResult_failure;
      } break;
      case AKIpUptaneMes_PR_sendFirmwareReq: {
        LOG_INFO << "Received sendFirmwareReq from Primary.";
        auto fw = msg->sendFirmwareReq();
        auto send_firmware_result = impl_.sendFirmware(ToString(fw->firmware));
        resp->present(AKIpUptaneMes_PR_sendFirmwareResp);
        auto r = resp->sendFirmwareResp();
        r->result = send_firmware_result ? AKInstallationResult_success : AKInstallationResult_failure;
      } break;
      case AKIpUptaneMes_PR_installReq: {
        LOG_INFO << "Received installReq from Primary.";
        auto request = msg->installReq();

        auto install_result = impl_.install(ToString(request->hash));

        resp->present(AKIpUptaneMes_PR_installResp);
        auto response_message = resp->installResp();
        response_message->result = static_cast<AKInstallationResultCode_t>(install_result);

        if (install_result == data::ResultCode::Numeric::kNeedCompletion) {
          need_reboot = true;
        }
      } break;
      default:
        LOG_ERROR << "Unrecognised message type:" << msg->present();
        return true;
    }

    // Send the response
    if (resp->present() != AKIpUptaneMes_PR_NOTHING) {
      int optval = 0;
      setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
      asn_enc_rval_t encode_result =
          der_encode(&asn_DEF_AKIpUptaneMes, &resp->msg_, Asn1SocketWriteCallback, reinterpret_cast<void *>(&socket));
      if (encode_result.encoded == -1) {
        return true;  // write error
      }
      optval = 1;
      setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
    } else {
      LOG_DEBUG << "Not sending a response to message " << msg->present();
    }

    if (need_reboot && reboot_after_install_) {
      exit_reason_ = ExitReason::kRebootNeeded;
      return false;
    }

  }  // Go back round and read another message

  return true;
  // Parse error => Shutdown the socket
  // write error => Shutdown the socket
  // Timeout on write => shutdown
}
