#include "secondary_ipc/aktualizr_secondary_ipc.h"

/* (De)serialization of KeyType might be moved to types.cc if needed elsewhere */
asn1::Serializer& operator<<(asn1::Serializer& ser, KeyType kt) {
  ser << asn1::implicit<kAsn1Enum>(static_cast<const int32_t&>(kt));
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, KeyType& kt) {
  int32_t kt_i;

  des >> asn1::implicit<kAsn1Enum>(kt_i);

  if (kt_i < kED25519 || kt_i > kRSA4096) throw deserialization_error();

  kt = static_cast<KeyType>(kt_i);

  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, SerializationFormat fmt) {
  ser << asn1::implicit<kAsn1Enum>(static_cast<const int32_t&>(fmt));
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SerializationFormat& fmt) {
  int32_t fmt_i = -1;

  des >> asn1::implicit<kAsn1Enum>(fmt_i);

  if (fmt_i < kSerializationJson || fmt_i > kSerializationBer) throw deserialization_error();

  fmt = static_cast<SerializationFormat>(fmt_i);

  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyReq& data) {
  (void)data;
  ser << asn1::seq << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyReq& data) {
  (void)data;
  des >> asn1::seq >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPublicKeyResp& data) {
  ser << asn1::seq << data.type << asn1::implicit<kAsn1OctetString>(data.key) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPublicKeyResp& data) {
  des >> asn1::seq >> data.type >> asn1::implicit<kAsn1OctetString>(data.key) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestReq& data) {
  (void)data;
  ser << asn1::seq << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestReq& data) {
  (void)data;
  des >> asn1::seq >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryManifestResp& data) {
  ser << asn1::seq << data.format << asn1::implicit<kAsn1OctetString>(data.manifest) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryManifestResp& data) {
  des >> asn1::seq >> data.format >> asn1::implicit<kAsn1OctetString>(data.manifest) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaReq& data) {
  ser << asn1::seq << data.director_targets_format << asn1::implicit<kAsn1OctetString>(data.director_targets);
  if (data.image_meta_present) {
    ser << asn1::seq << data.image_snapshot_format << asn1::implicit<kAsn1OctetString>(data.image_snapshot)
        << data.image_timestamp_format << asn1::implicit<kAsn1OctetString>(data.image_timestamp)
        << data.image_targets_format << asn1::implicit<kAsn1OctetString>(data.image_targets) << asn1::endseq;
  }
  ser << asn1::endseq;
  return ser;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutMetaReq& data) {
  des >> asn1::seq >> data.director_targets_format >> asn1::implicit<kAsn1OctetString>(data.director_targets) >>
      asn1::optional >> asn1::seq >> data.image_snapshot_format >>
      asn1::implicit<kAsn1OctetString>(data.image_snapshot) >> data.image_timestamp_format >>
      asn1::implicit<kAsn1OctetString>(data.image_timestamp) >> data.image_targets_format >>
      asn1::implicit<kAsn1OctetString>(data.image_targets) >> asn1::endseq >>
      asn1::endoptional(&data.image_meta_present) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutMetaResp& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Boolean>(data.result) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutMetaResp& data) {
  data.result = true;
  des >> asn1::seq >> asn1::optional >> asn1::implicit<kAsn1Boolean>(data.result) >> asn1::endoptional() >>
      asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionReq& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Boolean>(data.director) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionReq& data) {
  des >> asn1::seq >> asn1::implicit<kAsn1Boolean>(data.director) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryRootVersionResp& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Integer>(data.version) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryRootVersionResp& data) {
  des >> asn1::seq >> asn1::implicit<kAsn1Integer>(data.version) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootReq& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Boolean>(data.director) << data.root_format
      << asn1::implicit<kAsn1OctetString>(data.root) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootReq& data) {
  des >> asn1::seq >> asn1::implicit<kAsn1Boolean>(data.director) >> data.root_format >>
      asn1::implicit<kAsn1OctetString>(data.root) >> asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryPutRootResp& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Boolean>(data.result) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondaryPutRootResp& data) {
  data.result = true;
  des >> asn1::seq >> asn1::optional >> asn1::implicit<kAsn1Boolean>(data.result) >> asn1::endoptional() >>
      asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareReq& data) {
  ser << asn1::seq << asn1::implicit<kAsn1OctetString>(data.firmware) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareReq& data) {
  des >> asn1::seq >> asn1::optional >> asn1::implicit<kAsn1OctetString>(data.firmware) >> asn1::endoptional() >>
      asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareOstreeReq& data) {
  ser << asn1::seq << asn1::implicit<kAsn1OctetString>(data.cert_file)
      << asn1::implicit<kAsn1OctetString>(data.pkey_file) << asn1::implicit<kAsn1OctetString>(data.ca_file)
      << asn1::endseq;
  return ser;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareOstreeReq& data) {
  des >> asn1::seq >> asn1::implicit<kAsn1OctetString>(data.cert_file) >>
      asn1::implicit<kAsn1OctetString>(data.pkey_file) >> asn1::implicit<kAsn1OctetString>(data.ca_file) >>
      asn1::endseq;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondarySendFirmwareResp& data) {
  ser << asn1::seq << asn1::implicit<kAsn1Boolean>(data.result) << asn1::endseq;
  return ser;
}
asn1::Deserializer& operator>>(asn1::Deserializer& des, SecondarySendFirmwareResp& data) {
  data.result = true;
  des >> asn1::seq >> asn1::optional >> asn1::implicit<kAsn1Boolean>(data.result) >> asn1::endoptional() >>
      asn1::endseq;
  return des;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, std::unique_ptr<SecondaryMessage>& data) {
  uint8_t type_tag;
  ASN1_Class type_class;
  des >> asn1::peekexpl(&type_tag, &type_class);
  switch (type_class | type_tag) {
    case kAsn1Context | kSecondaryMesPublicKeyReqTag: {
      SecondaryPublicKeyReq* data_concrete = new SecondaryPublicKeyReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }

    case kAsn1Context | kSecondaryMesPublicKeyRespTag: {
      SecondaryPublicKeyResp* data_concrete = new SecondaryPublicKeyResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesManifestReqTag: {
      SecondaryManifestReq* data_concrete = new SecondaryManifestReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesManifestRespTag: {
      SecondaryManifestResp* data_concrete = new SecondaryManifestResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesPutMetaReqTag: {
      SecondaryPutMetaReq* data_concrete = new SecondaryPutMetaReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesPutMetaRespTag: {
      SecondaryPutMetaResp* data_concrete = new SecondaryPutMetaResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesRootVersionReqTag: {
      SecondaryRootVersionReq* data_concrete = new SecondaryRootVersionReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesRootVersionRespTag: {
      SecondaryRootVersionResp* data_concrete = new SecondaryRootVersionResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesPutRootReqTag: {
      SecondaryPutRootReq* data_concrete = new SecondaryPutRootReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesPutRootRespTag: {
      SecondaryPutRootResp* data_concrete = new SecondaryPutRootResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesSendFirmwareReqTag: {
      SecondarySendFirmwareReq* data_concrete = new SecondarySendFirmwareReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesSendFirmwareOstreeReqTag: {
      SecondarySendFirmwareOstreeReq* data_concrete = new SecondarySendFirmwareOstreeReq;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }
    case kAsn1Context | kSecondaryMesSendFirmwareRespTag: {
      SecondarySendFirmwareResp* data_concrete = new SecondarySendFirmwareResp;
      des >> *data_concrete;
      data = std::unique_ptr<SecondaryMessage>(data_concrete);
      break;
    }

    default:
      throw deserialization_error();
  }
  des >> asn1::endexpl;
  return des;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const SecondaryMessage& data) {
  ser << asn1::expl(data.mes_type);
  data.serialize(ser);
  ser << asn1::endexpl;
  return ser;
}
