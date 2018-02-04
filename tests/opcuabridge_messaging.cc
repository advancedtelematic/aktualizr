#include <gtest/gtest.h>

#include <open62541.h>

#define OPCUABRIDGE_ENABLE_SERIALIZATION

#include "utils.h"

#include "opcuabridge_test_utils.h"
#include "test_utils.h"

#include <fstream>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/bind.hpp>

TemporaryDirectory temp_dir("opcuabridge-messaging-test");

template <typename MessageT>
std::string GetMessageDumpFilePath() {
  return (temp_dir / GetMessageFileName<MessageT>()).native();
}

template <typename MessageT>
std::string GetMessageResponseFilePath() {
  return (temp_dir / (GetMessageFileName<MessageT>() + ".response")).native();
}

template <typename MessageT>
void LoadTransferCheck(UA_Client* client, const std::string& roottag) {
  std::string msg_content = GetMessageDumpFilePath<MessageT>();

  MessageT m, m_response;
  {
    std::ifstream file(msg_content.c_str());

    boost::archive::xml_iarchive ia(file);
    ia >> boost::serialization::make_nvp(roottag.c_str(), m);
  }

  UA_StatusCode retval = m.ClientWrite(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  retval = m_response.ClientRead(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  std::string msg_response = GetMessageResponseFilePath<MessageT>();
  {
    std::ofstream file(msg_response.c_str());

    boost::archive::xml_oarchive oa(file);
    oa << boost::serialization::make_nvp(roottag.c_str(), m_response);

    file.flush();
  }

  std::string diff_cmd = std::string("diff ") + " -q " + msg_content + " " + msg_response;
  EXPECT_EQ(0, system(diff_cmd.c_str()));
}

const std::string kHexValue = "00010203040506070809AABBCCDDEEFF";

opcuabridge::Signature createSignature(const std::string& keyid, const opcuabridge::SignatureMethod& method,
                                       const std::string& hash, const std::string& value) {
  opcuabridge::Signature s;
  s.set_keyid(keyid);
  s.set_method(method);
  opcuabridge::Hash h;
  h.set_function(opcuabridge::HASH_FUN_SHA256);
  h.set_digest(kHexValue);
  s.set_hash(h);
  s.set_value(value);
  return s;
}

opcuabridge::Signed createSigned(std::size_t n) {
  opcuabridge::Signed s;
  s.set_numberOfTokens(n);
  std::vector<int> tokens;
  for (int i = 0; i < n; ++i) tokens.push_back(i);
  s.set_tokens(tokens);
  s.set_timestamp(time(NULL));
  return s;
}

template <typename MessageT>
bool SerializeMessage(const std::string& roottag, const MessageT& m) {
  std::string filename = GetMessageDumpFilePath<MessageT>();
  std::ofstream file(filename.c_str());

  try {
    boost::archive::xml_oarchive oa(file);
    oa << boost::serialization::make_nvp(roottag.c_str(), m);
    file.flush();
  } catch (...) {
    return false;
  }
  return true;
}

TEST(opcuabridge, serialization) {
  boost::random::mt19937 gen;
  gen.seed(static_cast<unsigned int>(std::time(0))); 
  boost::random::uniform_int_distribution<uint8_t> random_byte(0x00,0xFF);

  opcuabridge::Signature s1 = createSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);
  opcuabridge::Signature s2 = createSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);

  std::vector<opcuabridge::Signature> signatures;
  signatures.push_back(s1);
  signatures.push_back(s2);

  opcuabridge::Signed s = createSigned(10);

  // VersionReport

  std::vector<opcuabridge::Hash> hashes;
  opcuabridge::Hash h;
  h.set_function(opcuabridge::HASH_FUN_SHA256);
  h.set_digest(kHexValue);
  hashes.push_back(h);

  opcuabridge::Image image;
  image.set_filename("IMAGE_FILENAME.EXT");
  image.set_length(0xffff);
  image.set_numberOfHashes(hashes.size());
  image.set_hashes(hashes);

  opcuabridge::ECUVersionManifestSigned ecu_version_manifest_signed;
  ecu_version_manifest_signed.set_ecuIdentifier("XXXXXXXX");
  ecu_version_manifest_signed.set_previousTime(0);
  ecu_version_manifest_signed.set_currentTime(time(NULL));
  ecu_version_manifest_signed.set_securityAttack("");
  ecu_version_manifest_signed.set_installedImage(image);

  opcuabridge::ECUVersionManifest ecu_version_manifest;
  ecu_version_manifest.set_numberOfSignatures(signatures.size());
  ecu_version_manifest.set_signatures(signatures);
  ecu_version_manifest.set_ecuVersionManifestSigned(ecu_version_manifest_signed);

  opcuabridge::VersionReport vr;
  vr.set_tokenForTimeServer(0);
  vr.set_ecuVersionManifest(ecu_version_manifest);

  EXPECT_TRUE(SerializeMessage("VersionReport", vr));

  // CurrentTime

  opcuabridge::CurrentTime ct;
  ct.set_numberOfSignatures(signatures.size());
  ct.set_signatures(signatures);
  ct.set_signed(s);

  EXPECT_TRUE(SerializeMessage("CurrentTime", ct));

  // MetadataFiles

  int guid = time(NULL);
  opcuabridge::MetadataFiles mds;
  mds.set_GUID(guid);
  mds.set_numberOfMetadataFiles(1);

  EXPECT_TRUE(SerializeMessage("MetadataFiles", mds));

  // MetadataFile

  opcuabridge::MetadataFile md;
  md.set_GUID(guid);
  md.set_fileNumber(1);
  md.set_filename("METADATA.EXT");
  std::vector<unsigned char> metadata(1024);
  std::generate(metadata.begin(), metadata.end(), boost::bind(random_byte, boost::ref(gen)));
  md.set_metadata(metadata);

  EXPECT_TRUE(SerializeMessage("MetadataFile", md));

  // ImageRequest

  opcuabridge::ImageRequest ir;
  ir.set_filename("IMAGE_FILE.EXT");

  EXPECT_TRUE(SerializeMessage("ImageRequest", ir));

  // ImageFile

  opcuabridge::ImageFile img_file;
  img_file.set_filename("IMAGE_FILE.EXT");
  img_file.set_numberOfBlocks(1);
  img_file.set_blockSize(1024);

  EXPECT_TRUE(SerializeMessage("ImageFile", img_file));

  // ImageBlock

  opcuabridge::ImageBlock img_block;
  img_block.set_filename("IMAGE_FILE.EXT");
  img_block.set_blockNumber(1);
  std::vector<unsigned char> block(1024);
  std::generate(block.begin(), block.end(), boost::bind(random_byte, boost::ref(gen)));
  img_block.set_block(block);

  EXPECT_TRUE(SerializeMessage("ImageBlock", img_block));
}

TEST(opcuabridge, transfer_messages) {
  std::string opc_port = TestUtils::getFreePort();
  std::string opc_url = std::string("opc.tcp://localhost:") + opc_port;

  TestHelperProcess server("./opcuabridge_server", opc_port);

  UA_Client* client = UA_Client_new(UA_ClientConfig_default);

  UA_StatusCode retval = UA_Client_connect(client, opc_url.c_str());

  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  BOOST_PP_LIST_FOR_EACH(LOAD_TRANSFER_CHECK, client, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  UA_Client_disconnect(client);
  UA_Client_delete(client);
}

#ifndef __NO_MAIN__
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
