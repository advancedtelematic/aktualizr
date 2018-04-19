#ifndef AKTUALIZR_SECONDARY_IPC_H
#define AKTUALIZR_SECONDARY_IPC_H

#include <memory>
#include <string>

#include <sys/socket.h>

#include "asn1/asn1-cerstream.h"
#include "channel.h"
#include "utilities/types.h"

enum SecondaryMesTypeTag {
  kSecondaryMesPublicKeyReqTag = 0x02,
  kSecondaryMesPublicKeyRespTag = 0x03,
  kSecondaryMesManifestReqTag = 0x04,
  kSecondaryMesManifestRespTag = 0x05,
  kSecondaryMesPutMetaReqTag = 0x06,
  kSecondaryMesPutMetaRespTag = 0x07,
  kSecondaryMesRootVersionReqTag = 0x08,
  kSecondaryMesRootVersionRespTag = 0x09,
  kSecondaryMesPutRootReqTag = 0x0a,
  kSecondaryMesPutRootRespTag = 0x0b,
  kSecondaryMesSendFirmwareReqTag = 0x0c,
  kSecondaryMesSendFirmwareRespTag = 0x0d,
  kSecondaryMesSendFirmwareOstreeReqTag = 0x0e
};

struct SecondaryMessage {
  SecondaryMesTypeTag mes_type;
  virtual ~SecondaryMessage() {}
  virtual void serialize(asn1::Serializer& ser) const = 0;
  virtual bool operator==(const SecondaryMessage& other) const { return mes_type == other.mes_type; }
};
asn1::Deserializer& operator>>(asn1::Deserializer& des, std::unique_ptr<SecondaryMessage>& data);
asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryMessage& data);

enum SerializationFormat {
  kSerializationJson = 0,
  kSerializationBer = 1,
};

struct SecondaryPublicKeyReq : public SecondaryMessage {
  SecondaryPublicKeyReq() { mes_type = kSecondaryMesPublicKeyReqTag; }
  ~SecondaryPublicKeyReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override { return SecondaryMessage::operator==(other); }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyReq& data);
};

struct SecondaryPublicKeyResp : public SecondaryMessage {
  SecondaryPublicKeyResp() { mes_type = kSecondaryMesPublicKeyRespTag; }
  ~SecondaryPublicKeyResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryPublicKeyResp& other_ = dynamic_cast<const SecondaryPublicKeyResp&>(other);
    return SecondaryMessage::operator==(other) && type == other_.type && key == other_.key;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyResp& data);
  KeyType type{kRSA2048};
  std::string key;
};

struct SecondaryManifestReq : public SecondaryMessage {
  SecondaryManifestReq() { mes_type = kSecondaryMesManifestReqTag; }
  ~SecondaryManifestReq() override {}
  bool operator==(const SecondaryMessage& other) const override { return SecondaryMessage::operator==(other); }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestReq& data);
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
};

struct SecondaryManifestResp : public SecondaryMessage {
  SecondaryManifestResp() { mes_type = kSecondaryMesManifestRespTag; }
  ~SecondaryManifestResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryManifestResp& other_ = dynamic_cast<const SecondaryManifestResp&>(other);
    return SecondaryMessage::operator==(other) && format == other_.format && manifest == other_.manifest;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestResp& data);
  SerializationFormat format{kSerializationJson};
  std::string manifest;
};

struct SecondaryPutMetaReq : public SecondaryMessage {
  SecondaryPutMetaReq() { mes_type = kSecondaryMesPutMetaReqTag; }
  ~SecondaryPutMetaReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryPutMetaReq& other_ = dynamic_cast<const SecondaryPutMetaReq&>(other);
    return SecondaryMessage::operator==(other) && director_targets_format == other_.director_targets_format &&
           director_targets == other_.director_targets && image_meta_present == other_.image_meta_present &&
           (!image_meta_present ||
            (image_snapshot_format == other_.image_snapshot_format && image_snapshot == other_.image_snapshot &&
             image_timestamp_format == other_.image_timestamp_format && image_timestamp == other_.image_timestamp &&
             image_targets_format == other_.image_targets_format && image_targets == other_.image_targets));
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutMetaReq& data);
  SerializationFormat director_targets_format{kSerializationJson};
  std::string director_targets;

  bool image_meta_present{true};
  SerializationFormat image_snapshot_format{kSerializationJson};
  std::string image_snapshot;
  SerializationFormat image_timestamp_format{kSerializationJson};
  std::string image_timestamp;
  SerializationFormat image_targets_format{kSerializationJson};
  std::string image_targets;
};

struct SecondaryPutMetaResp : public SecondaryMessage {
  SecondaryPutMetaResp() { mes_type = kSecondaryMesPutMetaRespTag; }
  ~SecondaryPutMetaResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryPutMetaResp& other_ = dynamic_cast<const SecondaryPutMetaResp&>(other);
    return SecondaryMessage::operator==(other) && result == other_.result;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestResp& data);
  bool result{true};
};

struct SecondaryRootVersionReq : public SecondaryMessage {
  SecondaryRootVersionReq() { mes_type = kSecondaryMesRootVersionReqTag; }
  ~SecondaryRootVersionReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryRootVersionReq& other_ = dynamic_cast<const SecondaryRootVersionReq&>(other);
    return SecondaryMessage::operator==(other) && director == other_.director;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionReq& data);
  bool director{true};
};

struct SecondaryRootVersionResp : public SecondaryMessage {
  SecondaryRootVersionResp() { mes_type = kSecondaryMesRootVersionRespTag; }
  ~SecondaryRootVersionResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryRootVersionResp& other_ = dynamic_cast<const SecondaryRootVersionResp&>(other);
    return SecondaryMessage::operator==(other) && version == other_.version;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionResp& data);
  int32_t version{-1};
};

struct SecondaryPutRootReq : public SecondaryMessage {
  SecondaryPutRootReq() { mes_type = kSecondaryMesPutRootReqTag; }
  ~SecondaryPutRootReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryPutRootReq& other_ = dynamic_cast<const SecondaryPutRootReq&>(other);
    return SecondaryMessage::operator==(other) && director == other_.director && root_format == other_.root_format &&
           root == other_.root;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootReq& data);
  bool director{true};

  SerializationFormat root_format{kSerializationJson};
  std::string root;
};

struct SecondaryPutRootResp : public SecondaryMessage {
  SecondaryPutRootResp() { mes_type = kSecondaryMesPutRootRespTag; }
  ~SecondaryPutRootResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondaryPutRootResp& other_ = dynamic_cast<const SecondaryPutRootResp&>(other);
    return SecondaryMessage::operator==(other) && result == other_.result;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootResp& data);
  bool result{true};
};

struct SecondarySendFirmwareOstreeReq : public SecondaryMessage {
  SecondarySendFirmwareOstreeReq() { mes_type = kSecondaryMesSendFirmwareOstreeReqTag; }
  ~SecondarySendFirmwareOstreeReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareOstreeReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareOstreeReq& data);

  std::string cert_file;
  std::string pkey_file;
  std::string ca_file;
};

struct SecondarySendFirmwareReq : public SecondaryMessage {
  SecondarySendFirmwareReq() { mes_type = kSecondaryMesSendFirmwareReqTag; }
  ~SecondarySendFirmwareReq() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondarySendFirmwareReq& other_ = dynamic_cast<const SecondarySendFirmwareReq&>(other);
    return SecondaryMessage::operator==(other) && firmware == other_.firmware;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareReq& data);

  std::string firmware;
};

struct SecondarySendFirmwareResp : public SecondaryMessage {
  SecondarySendFirmwareResp() { mes_type = kSecondaryMesSendFirmwareRespTag; }
  ~SecondarySendFirmwareResp() override {}
  void serialize(asn1::Serializer& ser) const override { ser << *this; }
  bool operator==(const SecondaryMessage& other) const override {
    const SecondarySendFirmwareResp& other_ = dynamic_cast<const SecondarySendFirmwareResp&>(other);
    return SecondaryMessage::operator==(other) && result == other_.result;
  }

  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareResp& data);
  bool result{true};
};

struct SecondaryPacket {
  SecondaryPacket(std::unique_ptr<SecondaryMessage>&& message) {
    msg = std::move(message);
    memset(&peer_addr, 0, sizeof(peer_addr));
  }
  SecondaryPacket(sockaddr_storage peer, std::unique_ptr<SecondaryMessage>&& message) : peer_addr(peer) {
    msg = std::move(message);
  }

  using ChanType = Channel<std::shared_ptr<SecondaryPacket>>;

  sockaddr_storage peer_addr;
  std::unique_ptr<SecondaryMessage> msg;
};

#endif  // AKTUALIZR_SECONDARY_IPC_H
