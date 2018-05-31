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

#define ASN1_MESSAGE_DEFINE_ACCESSOR(MessageType, FieldName) \
  SubPtr<MessageType> FieldName() { return Asn1Sub<MessageType>(this, &msg_.choice.FieldName); }
  AKIpUptaneMes_PR present() const { return msg_.present; }
  void present(AKIpUptaneMes_PR present) { msg_.present = present; }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKDiscoveryReqMes_t, discoveryReq);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKDiscoveryRespMes_t, discoveryResp);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN1_MESSAGE_DEFINE_ACCESSOR(AKPublicKeyReqMes_t, publicKeyReq);

  /**
   * The underlying message structure. This is public to simplify calls to
   * der_encode()/der_decode(). The Asn1<T> smart pointers should be used
   * in preference to poking around inside msg_.
   */
  AKIpUptaneMes_t msg_;

 private:
  int ref_count_{0};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  Asn1Message() { memset(&msg_, 0, sizeof(AKIpUptaneMes_t)); }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  explicit Asn1Message(AKIpUptaneMes_t** msg) {
    memmove(&msg_, *msg, sizeof(AKIpUptaneMes_t));
    free(*msg);  // Be careful. This needs to be the same free used in der_decode
    *msg = nullptr;
  }
};

/**
 * Adaptor to write output of der_encode to a string
 */
int WriteToString(const void* buffer, size_t size, void* priv);

/**
 * Convert OCTET_STRING_t into std::string
 */
std::string ToString(const OCTET_STRING_t& octet_str);

#endif  // ASN1_MESSAGE_H_