#include <gtest/gtest.h>

#include <open62541.h>

#define OPCUABRIDGE_ENABLE_SERIALIZATION

#include <functional>
#include "utilities/utils.h"

#include "opcuabridge_test_utils.h"
#include "test_utils.h"

#include <fstream>
#include <random>

namespace tutils = opcuabridge_test_utils;

TemporaryDirectory temp_dir("opcuabridge-messaging-test");

template <typename MessageT>
void LoadTransferCheck(UA_Client* client, const std::string& roottag) {
  std::string msg_content = tutils::GetMessageDumpFilePath<MessageT>(temp_dir);

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

  std::string msg_response = tutils::GetMessageResponseFilePath<MessageT>(temp_dir);
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

TEST(opcuabridge, serialization) {
  std::mt19937 gen;
  gen.seed(static_cast<unsigned int>(std::time(0)));
  std::uniform_int_distribution<uint8_t> random_byte(0x00, 0xFF);

  opcuabridge::Signature s1 = tutils::CreateSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);
  opcuabridge::Signature s2 = tutils::CreateSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);

  std::vector<opcuabridge::Signature> signatures;
  signatures.push_back(s1);
  signatures.push_back(s2);

  opcuabridge::Signed s = tutils::CreateSigned(10);

  // VersionReport

  std::vector<opcuabridge::Hash> hashes;
  opcuabridge::Hash h;
  h.setFunction(opcuabridge::HASH_FUN_SHA256);
  h.setDigest(kHexValue);
  hashes.push_back(h);

  opcuabridge::Image image;
  image.setFilename("IMAGE_FILENAME.EXT");
  image.setLength(0xffff);
  image.setHashes(hashes);

  opcuabridge::ECUVersionManifestSigned ecu_version_manifest_signed;
  ecu_version_manifest_signed.setEcuIdentifier("XXXXXXXX");
  ecu_version_manifest_signed.setPreviousTime(0);
  ecu_version_manifest_signed.setCurrentTime(time(NULL));
  ecu_version_manifest_signed.setSecurityAttack("");
  ecu_version_manifest_signed.setInstalledImage(image);

  opcuabridge::ECUVersionManifest ecu_version_manifest;
  ecu_version_manifest.setSignatures(signatures);
  ecu_version_manifest.setEcuVersionManifestSigned(ecu_version_manifest_signed);

  opcuabridge::VersionReport vr;
  vr.setTokenForTimeServer(0);
  vr.setEcuVersionManifest(ecu_version_manifest);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "VersionReport", vr));

  // CurrentTime

  opcuabridge::CurrentTime ct;
  ct.setSignatures(signatures);
  ct.setSigned(s);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "CurrentTime", ct));

  // MetadataFiles

  int guid = time(NULL);
  opcuabridge::MetadataFiles mds;
  mds.setGUID(guid);
  mds.setNumberOfMetadataFiles(1);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "MetadataFiles", mds));

  // MetadataFile

  opcuabridge::MetadataFile md;
  md.setGUID(guid);
  md.setFileNumber(1);
  md.setFilename("METADATA.EXT");
  std::vector<unsigned char> metadata(1024);
  std::generate(metadata.begin(), metadata.end(), std::bind(random_byte, std::ref(gen)));
  md.setMetadata(metadata);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "MetadataFile", md));

  // ImageRequest

  opcuabridge::ImageRequest ir;
  ir.setFilename("IMAGE_FILE.EXT");

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "ImageRequest", ir));

  // ImageFile

  opcuabridge::ImageFile img_file;
  img_file.setFilename("IMAGE_FILE.EXT");
  img_file.setNumberOfBlocks(1);
  img_file.setBlockSize(1024);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "ImageFile", img_file));

  // ImageBlock

  opcuabridge::ImageBlock img_block;
  img_block.setFilename("IMAGE_FILE.EXT");
  img_block.setBlockNumber(1);
  std::vector<unsigned char> block(1024);
  std::generate(block.begin(), block.end(), std::bind(random_byte, std::ref(gen)));
  img_block.setBlock(block);

  EXPECT_TRUE(tutils::SerializeMessage(temp_dir, "ImageBlock", img_block));
}

TEST(opcuabridge, transfer_messages) {
  std::string opc_port = TestUtils::getFreePort();
  std::string opc_url = std::string("opc.tcp://localhost:") + opc_port;

  TestHelperProcess server("./opcuabridge-server", opc_port);

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
