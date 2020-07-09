/**
 * \file
 */

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include "libaktualizr/config.h"

#include "asn1-cerstream.h"
#include "asn1_message.h"
#include "der_encoder.h"
#include "utilities/utils.h"

asn1::Serializer& operator<<(asn1::Serializer& ser, CryptoSource cs) {
  ser << asn1::implicit<kAsn1Enum>(static_cast<const int32_t&>(static_cast<int>(cs)));

  return ser;
}

asn1::Serializer& operator<<(asn1::Serializer& ser, const TlsConfig& tls_conf) {
  ser << asn1::seq << asn1::implicit<kAsn1Utf8String>(tls_conf.server)
      << asn1::implicit<kAsn1Utf8String>(tls_conf.server_url_path.string()) << tls_conf.ca_source
      << tls_conf.pkey_source << tls_conf.cert_source << asn1::endseq;
  return ser;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, CryptoSource& cs) {
  int32_t cs_i;
  des >> asn1::implicit<kAsn1Enum>(cs_i);

  if (cs_i < static_cast<int>(CryptoSource::kFile) || cs_i > static_cast<int>(CryptoSource::kPkcs11)) {
    throw deserialization_error();
  }

  cs = static_cast<CryptoSource>(cs_i);

  return des;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, boost::filesystem::path& path) {
  std::string path_string;
  des >> asn1::implicit<kAsn1Utf8String>(path_string);
  path = path_string;
  return des;
}

asn1::Deserializer& operator>>(asn1::Deserializer& des, TlsConfig& tls_conf) {
  des >> asn1::seq >> asn1::implicit<kAsn1Utf8String>(tls_conf.server) >> tls_conf.server_url_path >>
      tls_conf.ca_source >> tls_conf.pkey_source >> tls_conf.cert_source >> asn1::endseq;
  return des;
}

void printStringHex(const std::string& s) {
  for (char c : s) {
    std::cerr << std::setfill('0') << std::setw(2) << std::hex << (((unsigned int)c) & 0xFF);
    std::cerr << ' ';
  }

  std::cerr << std::dec << std::endl;
}

std::string CCString(OCTET_STRING_t par) { return std::string((const char*)par.buf, (size_t)par.size); }
bool operator==(const AKTlsConfig& cc_config, const TlsConfig& config) {
  if (config.server != CCString(cc_config.server)) return false;
  if (config.server_url_path.string() != CCString(cc_config.serverUrlPath)) return false;
  if (static_cast<int>(config.ca_source) != cc_config.caSource) return false;
  if (static_cast<int>(config.pkey_source) != cc_config.pkeySource) return false;
  if (static_cast<int>(config.cert_source) != cc_config.certSource) return false;
  return true;
}

bool operator==(const TlsConfig& config, const AKTlsConfig& cc_config) { return cc_config == config; }

TEST(asn1_config, tls_config) {
  TlsConfig conf;

  conf.server = "https://example.com";
  conf.server_url_path = "";
  conf.ca_source = CryptoSource::kFile;
  conf.pkey_source = CryptoSource::kPkcs11;
  conf.cert_source = CryptoSource::kPkcs11;

  asn1::Serializer ser;
  ser << conf;
  asn1::Deserializer des(ser.getResult());

  TlsConfig conf2;
  EXPECT_NO_THROW(des >> conf2);
  EXPECT_EQ(conf.server, conf2.server);
  EXPECT_EQ(conf.server_url_path, conf2.server_url_path);
  EXPECT_EQ(conf.ca_source, conf2.ca_source);
  EXPECT_EQ(conf.pkey_source, conf2.pkey_source);
  EXPECT_EQ(conf.cert_source, conf2.cert_source);
}

TEST(asn1_config, tls_config_asn1cc_to_man) {
  AKTlsConfig_t cc_tls_conf;
  memset(&cc_tls_conf, 0, sizeof(cc_tls_conf));

  std::string server = "https://example.com";
  EXPECT_EQ(0, OCTET_STRING_fromBuf(&cc_tls_conf.server, server.c_str(), static_cast<int>(server.length())));

  std::string server_url_path = "";
  EXPECT_EQ(0, OCTET_STRING_fromBuf(&cc_tls_conf.serverUrlPath, server_url_path.c_str(),
                                    static_cast<int>(server_url_path.length())));

  cc_tls_conf.caSource = static_cast<int>(CryptoSource::kFile);
  cc_tls_conf.pkeySource = static_cast<int>(CryptoSource::kPkcs11);
  cc_tls_conf.certSource = static_cast<int>(CryptoSource::kPkcs11);

  asn_enc_rval_t enc;
  std::string der;

  enc = der_encode(&asn_DEF_AKTlsConfig, &cc_tls_conf, Asn1StringAppendCallback, &der);
  EXPECT_NE(enc.encoded, -1);

  TlsConfig conf;
  asn1::Deserializer des(der);
  EXPECT_NO_THROW(des >> conf);
  EXPECT_EQ(conf, cc_tls_conf);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_AKTlsConfig, &cc_tls_conf);
}

TEST(asn1_config, tls_config_man_to_asn1cc) {
  TlsConfig conf;

  conf.server = "https://example.com";
  conf.server_url_path = "";
  conf.ca_source = CryptoSource::kFile;
  conf.pkey_source = CryptoSource::kPkcs11;
  conf.cert_source = CryptoSource::kPkcs11;

  asn1::Serializer ser;

  ser << conf;

  AKTlsConfig_t* cc_tls_conf = nullptr;
  asn_dec_rval_t ret =
      ber_decode(0, &asn_DEF_AKTlsConfig, (void**)&cc_tls_conf, ser.getResult().c_str(), ser.getResult().length());
  EXPECT_EQ(ret.code, RC_OK);
  EXPECT_EQ(*cc_tls_conf, conf);
  ASN_STRUCT_FREE(asn_DEF_AKTlsConfig, cc_tls_conf);
}

TEST(asn1_common, longstring) {
  std::string in =
      "-----BEGIN PUBLIC KEY-----\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAumdoILJANzcKUn0IZi1B\
OB6jj0uE5XrZPTbUuQT8jsA+rYNet1VF1Y0X8/hftShHzL8M+X9rlEwvnAhzdWKd\
IEQUjfuiJIOLBtAGZZNYdTTXx7sFQ/UQwKo8mU6vSMqsbOdzidp6SpRRiEHpWH4m\
rvurn/jWPAVY2vwD0VxUBl1ps/C4qYGqeRQz7o7SAgV3NPDZLPbKVz9+YH+tkVR+\
FMsH9/YebTpaiL8uQsf24WdeVUc7WCJLzOTvPh+FnNB2y78ye29sIwHpbiivmfrO\
GSdjzMzSMr0UATqOXcaONhPKGNDQ3jhTCayi/lryYBgpRyvSLRpaIlaS0dLtp7Zp\
zQIDAQAB\
-----END PUBLIC KEY-----";

  asn1::Serializer ser;
  ser << asn1::implicit<kAsn1OctetString>(in);
  asn1::Deserializer des(ser.getResult());

  std::string out;
  EXPECT_NO_THROW(des >> asn1::implicit<kAsn1OctetString>(out));
  EXPECT_EQ(in, out);
}

TEST(asn1_common, longlongstring) {
  std::string in =
      "-----BEGIN PUBLIC KEY-----\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAumdoILJANzcKUn0IZi1B\
OB6jj0uE5XrZPTbUuQT8jsA+rYNet1VF1Y0X8/hftShHzL8M+X9rlEwvnAhzdWKd\
IEQUjfuiJIOLBtAGZZNYdTTXx7sFQ/UQwKo8mU6vSMqsbOdzidp6SpRRiEHpWH4m\
rvurn/jWPAVY2vwD0VxUBl1ps/C4qYGqeRQz7o7SAgV3NPDZLPbKVz9+YH+tkVR+\
FMsH9/YebTpaiL8uQsf24WdeVUc7WCJLzOTvPh+FnNB2y78ye29sIwHpbiivmfrO\
GSdjzMzSMr0UATqOXcaONhPKGNDQ3jhTCayi/lryYBgpRyvSLRpaIlaS0dLtp7Zp\
zQIDAQAB\
-----END PUBLIC KEY-----\
-----BEGIN PUBLIC KEY-----\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAumdoILJANzcKUn0IZi1B\
OB6jj0uE5XrZPTbUuQT8jsA+rYNet1VF1Y0X8/hftShHzL8M+X9rlEwvnAhzdWKd\
IEQUjfuiJIOLBtAGZZNYdTTXx7sFQ/UQwKo8mU6vSMqsbOdzidp6SpRRiEHpWH4m\
rvurn/jWPAVY2vwD0VxUBl1ps/C4qYGqeRQz7o7SAgV3NPDZLPbKVz9+YH+tkVR+\
FMsH9/YebTpaiL8uQsf24WdeVUc7WCJLzOTvPh+FnNB2y78ye29sIwHpbiivmfrO\
GSdjzMzSMr0UATqOXcaONhPKGNDQ3jhTCayi/lryYBgpRyvSLRpaIlaS0dLtp7Zp\
zQIDAQAB\
-----END PUBLIC KEY-----\
-----BEGIN PUBLIC KEY-----\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAumdoILJANzcKUn0IZi1B\
OB6jj0uE5XrZPTbUuQT8jsA+rYNet1VF1Y0X8/hftShHzL8M+X9rlEwvnAhzdWKd\
IEQUjfuiJIOLBtAGZZNYdTTXx7sFQ/UQwKo8mU6vSMqsbOdzidp6SpRRiEHpWH4m\
rvurn/jWPAVY2vwD0VxUBl1ps/C4qYGqeRQz7o7SAgV3NPDZLPbKVz9+YH+tkVR+\
FMsH9/YebTpaiL8uQsf24WdeVUc7WCJLzOTvPh+FnNB2y78ye29sIwHpbiivmfrO\
GSdjzMzSMr0UATqOXcaONhPKGNDQ3jhTCayi/lryYBgpRyvSLRpaIlaS0dLtp7Zp\
zQIDAQAB\
-----END PUBLIC KEY-----\
-----BEGIN PUBLIC KEY-----\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAumdoILJANzcKUn0IZi1B\
OB6jj0uE5XrZPTbUuQT8jsA+rYNet1VF1Y0X8/hftShHzL8M+X9rlEwvnAhzdWKd\
IEQUjfuiJIOLBtAGZZNYdTTXx7sFQ/UQwKo8mU6vSMqsbOdzidp6SpRRiEHpWH4m\
rvurn/jWPAVY2vwD0VxUBl1ps/C4qYGqeRQz7o7SAgV3NPDZLPbKVz9+YH+tkVR+\
FMsH9/YebTpaiL8uQsf24WdeVUc7WCJLzOTvPh+FnNB2y78ye29sIwHpbiivmfrO\
GSdjzMzSMr0UATqOXcaONhPKGNDQ3jhTCayi/lryYBgpRyvSLRpaIlaS0dLtp7Zp\
zQIDAQAB\
-----END PUBLIC KEY-----";

  asn1::Serializer ser;
  ser << asn1::implicit<kAsn1OctetString>(in);
  asn1::Deserializer des(ser.getResult());

  std::string out;
  EXPECT_NO_THROW(des >> asn1::implicit<kAsn1OctetString>(out));
  EXPECT_EQ(in, out);
}

TEST(asn1_common, Asn1MessageSimple) {
  // Fill in a message
  Asn1Message::Ptr original(Asn1Message::Empty());
  original->present(AKIpUptaneMes_PR_getInfoResp);
  Asn1Message::SubPtr<AKGetInfoRespMes> req = original->getInfoResp();
  SetString(&req->ecuSerial, "serial1234");
  SetString(&req->hwId, "hd-id-001");

  // BER encode
  std::string buffer;
  der_encode(&asn_DEF_AKIpUptaneMes, &original->msg_, Asn1StringAppendCallback, &buffer);

  EXPECT_GT(buffer.size(), 0);

  // BER decode
  asn_codec_ctx_t context;
  memset(&context, 0, sizeof(context));

  AKIpUptaneMes_t* m = nullptr;
  asn_dec_rval_t res =
      ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void**>(&m), buffer.c_str(), buffer.size());
  Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);

  // Check decoding succeeded
  EXPECT_EQ(res.code, RC_OK);
  EXPECT_EQ(res.consumed, buffer.size());

  // Check results are what we started with
  EXPECT_EQ(msg->present(), AKIpUptaneMes_PR_getInfoResp);
  Asn1Message::SubPtr<AKGetInfoRespMes> resp = msg->getInfoResp();
  msg.reset();  // Asn1Message::SubPtr<T> keeps the root object alive
  EXPECT_EQ(ToString(resp->ecuSerial), "serial1234");
  EXPECT_EQ(ToString(resp->hwId), "hd-id-001");
}

TEST(asn1_common, parse) {
  std::string data = Utils::fromBase64("pgkwBwQFaGVsbG8=");
  // BER decode
  asn_codec_ctx_t context;
  memset(&context, 0, sizeof(context));

  AKIpUptaneMes_t* m = nullptr;
  asn_dec_rval_t res =
      ber_decode(&context, &asn_DEF_AKIpUptaneMes, reinterpret_cast<void**>(&m), data.c_str(), data.size());
  Asn1Message::Ptr msg = Asn1Message::FromRaw(&m);
  EXPECT_EQ(res.code, RC_OK);
  EXPECT_EQ(AKIpUptaneMes_PR_sendFirmwareReq, msg->present());
}

TEST(asn1_common, Asn1MessageFromRawNull) {
  Asn1Message::FromRaw(nullptr);
  AKIpUptaneMes_t* m = nullptr;
  Asn1Message::FromRaw(&m);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
