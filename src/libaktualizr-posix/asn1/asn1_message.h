#ifndef ASN1_MESSAGE_H_
#define ASN1_MESSAGE_H_
#include <boost/intrusive_ptr.hpp>

#include "AKIpUptaneMes.h"
#include "AKTlsConfig.h"

class Asn1Message;

template <typename T>
class Asn1Sub {
 public:
  Asn1Sub(boost::intrusive_ptr<Asn1Message> root, T* me) : root_(std::move(root)), me_(me) {}

  T& operator*() const {
    assert(me_ != nullptr);
    return *me_;
  }

  T* operator->() const {
    assert(me_ != nullptr);
    return me_;
  }

 private:
  boost::intrusive_ptr<Asn1Message> root_;
  T* me_;
};

/**
 * Reference counted holder for the top-level ASN1 message structure.
 */

class Asn1Message {
 public:
  using Ptr = boost::intrusive_ptr<Asn1Message>;
  template <typename T>
  using SubPtr = Asn1Sub<T>;

  Asn1Message(const Asn1Message&) = delete;
  Asn1Message operator=(const Asn1Message&) = delete;

  /**
   * Create a new Asn1Message, in order to fill it with data and send it
   */
  static Asn1Message::Ptr Empty() { return new Asn1Message(); }

  /**
   * Destructively copy from a raw msg pointer created by parsing an incomming
   * message. This takes ownership of the contents of the message, and sets
   * *msg=nullptr to make this fact clear.
   */
  static Asn1Message::Ptr FromRaw(AKIpUptaneMes_t** msg) { return new Asn1Message(msg); }

  ~Asn1Message() { ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_AKIpUptaneMes, &msg_); }
  friend void intrusive_ptr_add_ref(Asn1Message* m) { m->ref_count_++; }
  friend void intrusive_ptr_release(Asn1Message* m) {
    if (--m->ref_count_ == 0) {
      delete m;
    }
  }

  AKIpUptaneMes_PR present() const { return msg_.present; }
  Asn1Message& present(AKIpUptaneMes_PR present) {
    msg_.present = present;
    return *this;
  }

#define ASN1_MESSAGE_DEFINE_ACCESSOR(MessageType, FieldName)                                                         \
  SubPtr<MessageType> FieldName() {                                                                                  \
    return Asn1Sub<MessageType>(this, &msg_.choice.FieldName); /* NOLINT(cppcoreguidelines-pro-type-union-access) */ \
  }

  ASN1_MESSAGE_DEFINE_ACCESSOR(AKGetInfoReqMes_t, getInfoReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKGetInfoRespMes_t, getInfoResp);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKManifestReqMes_t, manifestReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKManifestRespMes_t, manifestResp);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKPutMetaReqMes_t, putMetaReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKPutMetaRespMes_t, putMetaResp);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKSendFirmwareReqMes_t, sendFirmwareReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKSendFirmwareRespMes_t, sendFirmwareResp);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKInstallReqMes_t, installReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKInstallRespMes_t, installResp);

  ASN1_MESSAGE_DEFINE_ACCESSOR(AKUploadDataReqMes_t, uploadDataReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKUploadDataRespMes_t, uploadDataResp);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKDownloadOstreeRevReqMes_t, downloadOstreeRevReq);
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKDownloadOstreeRevRespMes_t, downloadOstreeRevResp);

#define ASN1_MESSAGE_DEFINE_STR_NAME(MessageID) \
  case MessageID:                               \
    return #MessageID;

  const char* toStr() {
    switch (present()) {
      default:
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_NOTHING);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_getInfoReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_getInfoResp);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_manifestReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_manifestResp);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_putMetaReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_putMetaResp);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_sendFirmwareReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_sendFirmwareResp);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_installReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_installResp);

        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_uploadDataReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_uploadDataResp);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_downloadOstreeRevReq);
        ASN1_MESSAGE_DEFINE_STR_NAME(AKIpUptaneMes_PR_downloadOstreeRevResp);
    }
    return "Unknown";
  };

  /**
   * The underlying message structure. This is public to simplify calls to
   * der_encode()/der_decode(). The Asn1<T> smart pointers should be used
   * in preference to poking around inside msg_.
   */
  AKIpUptaneMes_t msg_{};  // Note that this must be zero-initialized

 private:
  int ref_count_{0};

  Asn1Message() = default;

  explicit Asn1Message(AKIpUptaneMes_t** msg) {
    if (msg != nullptr && *msg != nullptr) {
      memmove(&msg_, *msg, sizeof(AKIpUptaneMes_t));
      free(*msg);  // Be careful. This needs to be the same free() used in der_decode
      *msg = nullptr;
    }
  }
};

/**
 * Adaptor to write output of der_encode to a string
 */
int Asn1StringAppendCallback(const void* buffer, size_t size, void* priv);

/**
 * Adaptor to write output of der_encode to a socket
 * priv is a pointer to an int holding the fd
 */
int Asn1SocketWriteCallback(const void* buffer, size_t size, void* priv);

/**
 * Convert OCTET_STRING_t into std::string
 */
std::string ToString(const OCTET_STRING_t& octet_str);

void SetString(OCTET_STRING_t* dest, const std::string& str);

/**
 * Open a TCP connection to client; send a message and wait for a
 * response.
 */
Asn1Message::Ptr Asn1Rpc(const Asn1Message::Ptr& tx, int con_fd);
Asn1Message::Ptr Asn1Rpc(const Asn1Message::Ptr& tx, const std::pair<std::string, uint16_t>& addr);
#endif  // ASN1_MESSAGE_H_
