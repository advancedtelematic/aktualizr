#ifndef AKTUALIZR_SECONDARY_IPC_H
#define AKTUALIZR_SECONDARY_IPC_H

#include <memory>
#include <string>

#include <sys/socket.h>

#include "asn1-cerstream.h"
#include "channel.h"
#include "types.h"

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
};

struct SecondaryMessage {
  SecondaryMesTypeTag mes_type;
  virtual void serialize(asn1::Serializer& ser) const = 0;
};
asn1::Deserializer& operator>>(asn1::Deserializer& des, std::unique_ptr<SecondaryMessage>& data);
asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryMessage& data);

enum SerializationFormat {
  kSerializationJson = 0,
  kSerializationBer = 1,
};

struct SecondaryPublicKeyReq : public SecondaryMessage {
  SecondaryPublicKeyReq() { mes_type = kSecondaryMesPublicKeyReqTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyReq& data);
};

struct SecondaryPublicKeyResp : public SecondaryMessage {
  SecondaryPublicKeyResp() { mes_type = kSecondaryMesPublicKeyRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyResp& data);
  KeyType type;
  std::string key;
};

struct SecondaryManifestReq : public SecondaryMessage {
  SecondaryManifestReq() { mes_type = kSecondaryMesManifestReqTag; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestReq& data);
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
};

struct SecondaryManifestResp : public SecondaryMessage {
  SecondaryManifestResp() { mes_type = kSecondaryMesManifestRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestResp& data);
  SerializationFormat format;
  std::string manifest;
};

struct SecondaryPutMetaReq : public SecondaryMessage {
  SecondaryPutMetaReq() { mes_type = kSecondaryMesPutMetaReqTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutMetaReq& data);
  SerializationFormat director_targets_format;
  std::string director_targets;

  bool image_meta_present;
  SerializationFormat image_snapshot_format;
  std::string image_snapshot;
  SerializationFormat image_timestamp_format;
  std::string image_timestamp;
  SerializationFormat image_targets_format;
  std::string image_targets;
};

struct SecondaryPutMetaResp : public SecondaryMessage {
  SecondaryPutMetaResp() { mes_type = kSecondaryMesPutMetaRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestResp& data);
  bool result = true;
};

struct SecondaryRootVersionReq : public SecondaryMessage {
  SecondaryRootVersionReq() { mes_type = kSecondaryMesRootVersionReqTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionReq& data);
  bool director;
};

struct SecondaryRootVersionResp : public SecondaryMessage {
  SecondaryRootVersionResp() { mes_type = kSecondaryMesRootVersionRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionResp& data);
  int32_t version;
};

struct SecondaryPutRootReq : public SecondaryMessage {
  SecondaryPutRootReq() { mes_type = kSecondaryMesPutRootReqTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootReq& data);
  bool director;

  SerializationFormat root_format;
  std::string root;
};

struct SecondaryPutRootResp : public SecondaryMessage {
  SecondaryPutRootResp() { mes_type = kSecondaryMesPutRootRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootResp& data);
  bool result = true;
};

struct SecondarySendFirmwareReq : public SecondaryMessage {
  SecondarySendFirmwareReq() { mes_type = kSecondaryMesSendFirmwareReqTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareReq& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareReq& data);

  std::string firmware;
};

struct SecondarySendFirmwareResp : public SecondaryMessage {
  SecondarySendFirmwareResp() { mes_type = kSecondaryMesSendFirmwareRespTag; }
  virtual void serialize(asn1::Serializer& ser) const { ser << *this; }
  friend asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareResp& data);
  friend asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareResp& data);
  bool result = true;
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
