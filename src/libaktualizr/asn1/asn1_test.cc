/**
 * \file
 */

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "AKIpUptaneMes.h"
#include "AKTlsConfig.h"
#include "config/config.h"
#include "secondary_ipc/aktualizr_secondary_ipc.h"

namespace bpo = boost::program_options;
boost::filesystem::path build_dir;

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
  if (config.ca_source != cc_config.caSource) return false;
  if (config.pkey_source != cc_config.pkeySource) return false;
  if (config.cert_source != cc_config.certSource) return false;
  return true;
}

bool operator==(const TlsConfig& config, const AKTlsConfig& cc_config) { return cc_config == config; }
static int write_out(const void* buffer, size_t size, void* priv) {
  std::string* out_str = (std::string*)priv;
  out_str->append(std::string((const char*)buffer, size));

  return 0;
}

TEST(asn1_config, tls_config) {
  TlsConfig conf;

  conf.server = "https://example.com";
  conf.server_url_path = "";
  conf.ca_source = kFile;
  conf.pkey_source = kPkcs11;
  conf.cert_source = kPkcs11;

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
  EXPECT_EQ(0, OCTET_STRING_fromBuf(&cc_tls_conf.server, server.c_str(), server.length()));

  std::string server_url_path = "";
  EXPECT_EQ(0, OCTET_STRING_fromBuf(&cc_tls_conf.serverUrlPath, server_url_path.c_str(), server_url_path.length()));

  cc_tls_conf.caSource = kFile;
  cc_tls_conf.pkeySource = kPkcs11;
  cc_tls_conf.certSource = kPkcs11;

  asn_enc_rval_t enc;
  std::string der;

  enc = der_encode(&asn_DEF_AKTlsConfig, &cc_tls_conf, write_out, &der);
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
  conf.ca_source = kFile;
  conf.pkey_source = kPkcs11;
  conf.cert_source = kPkcs11;

  asn1::Serializer ser;

  ser << conf;

  AKTlsConfig_t* cc_tls_conf = nullptr;
  asn_dec_rval_t ret =
      ber_decode(0, &asn_DEF_AKTlsConfig, (void**)&cc_tls_conf, ser.getResult().c_str(), ser.getResult().length());
  EXPECT_EQ(ret.code, RC_OK);
  EXPECT_EQ(*cc_tls_conf, conf);
  ASN_STRUCT_FREE(asn_DEF_AKTlsConfig, cc_tls_conf);
}

TEST(asn1_uptane_ip, public_key_req) {
  SecondaryPublicKeyReq mes;
  SecondaryMessage& mes_r = mes;

  asn1::Serializer ser;
  ser << mes_r;
  asn1::Deserializer des(ser.getResult());

  std::unique_ptr<SecondaryMessage> mes2;
  EXPECT_NO_THROW(des >> mes2);
  EXPECT_EQ(mes2->mes_type, mes_r.mes_type);
}

TEST(asn1_uptane_ip, public_key_req_asn1cc_to_man) {
  AKIpUptaneMes_t cc_mes;
  memset(&cc_mes, 0, sizeof(cc_mes));

  asn_enc_rval_t enc;
  std::string der;

  cc_mes.present = AKIpUptaneMes_PR_publicKeyReq;
  enc = der_encode(&asn_DEF_AKIpUptaneMes, &cc_mes, write_out, &der);
  EXPECT_NE(enc.encoded, -1);

  std::unique_ptr<SecondaryMessage> mes;
  asn1::Deserializer des(der);
  EXPECT_NO_THROW(des >> mes);
  EXPECT_EQ(mes->mes_type, kSecondaryMesPublicKeyReqTag);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_AKIpUptaneMes, &mes);
}

TEST(asn1_uptane_ip, public_key_req_man_to_asn1cc) {
  SecondaryPublicKeyReq mes;
  SecondaryMessage& mes_r = mes;

  asn1::Serializer ser;
  ser << mes_r;

  AKIpUptaneMes_t* cc_mes = nullptr;
  asn_dec_rval_t ret =
      ber_decode(0, &asn_DEF_AKIpUptaneMes, (void**)&cc_mes, ser.getResult().c_str(), ser.getResult().length());
  EXPECT_EQ(ret.code, RC_OK);
  EXPECT_EQ(cc_mes->present, AKIpUptaneMes_PR_publicKeyReq);
  ASN_STRUCT_FREE(asn_DEF_AKIpUptaneMes, cc_mes);
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

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the build directory as an input argument.\n";
    return EXIT_FAILURE;
  }
  build_dir = argv[1];
  return RUN_ALL_TESTS();
}
#endif
