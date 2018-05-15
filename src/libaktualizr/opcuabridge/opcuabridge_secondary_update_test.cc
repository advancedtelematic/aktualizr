#include <gtest/gtest.h>

#include <open62541.h>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>

#define OPCUABRIDGE_ENABLE_SERIALIZATION

#include "utilities/utils.h"

#include "opcuabridge_test_utils.h"
#include "test_utils.h"

#include "crypto/crypto.h"

#include <boost/filesystem.hpp>

namespace tutils = opcuabridge_test_utils;
namespace fs = boost::filesystem;

TemporaryDirectory temp_dir("opcuabridge-secondary-update-test");

const char* kMetadataFileName = "METADATA.EXT";
const char* kUpdateImageFileName = "UPDATE_IMAGE.EXT";
const std::size_t kUpdateImageBlockSize = 1024;
const std::size_t kUpdateImageSize = 10 * kUpdateImageBlockSize;

const std::string kHexValue = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

fs::path kImageFilePath = temp_dir / kUpdateImageFileName;
fs::path kMetadataFilePath = temp_dir / kMetadataFileName;

TEST(opcuabridge, prepare_update) {
  std::mt19937 gen;
  gen.seed(static_cast<unsigned int>(std::time(0)));
  std::uniform_int_distribution<uint8_t> random_byte(0x00, 0xFF);

  std::vector<unsigned char> block(kUpdateImageBlockSize);
  std::ofstream imagefile(kImageFilePath.c_str(), std::ios::binary | std::ios::app);
  MultiPartSHA256Hasher hasher;
  std::size_t size = 0;
  while (size < kUpdateImageSize) {
    std::generate(block.begin(), block.end(), std::bind(random_byte, std::ref(gen)));
    hasher.update(&block[0], block.size());
    std::copy(block.begin(), block.end(), std::ostreambuf_iterator<char>(imagefile.rdbuf()));
    size += std::min(block.size(), kUpdateImageSize - size);
  }

  std::ofstream mdfile(kMetadataFilePath.c_str());
  mdfile << kUpdateImageFileName << std::endl;
  mdfile << kUpdateImageSize << std::endl;
  mdfile << hasher.getHexDigest() << std::endl;

  EXPECT_TRUE(fs::exists(kImageFilePath));
  EXPECT_TRUE(fs::exists(kMetadataFilePath));
}

TEST(opcuabridge, secondary_update) {
  std::string opc_port = TestUtils::getFreePort();
  std::string opc_url = std::string("opc.tcp://localhost:") + opc_port;

  TestHelperProcess server("./opcuabridge-secondary", opc_port);

  UA_Client* client = UA_Client_new(UA_ClientConfig_default);

  UA_StatusCode retval = UA_Client_connect(client, opc_url.c_str());
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  // read initial version report
  opcuabridge::VersionReport initial_vr;
  retval = initial_vr.ClientRead(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  // write current time
  opcuabridge::Signature s1 = tutils::CreateSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);
  opcuabridge::Signature s2 = tutils::CreateSignature(kHexValue, opcuabridge::SIG_METHOD_ED25519, kHexValue, kHexValue);

  std::vector<opcuabridge::Signature> signatures;
  signatures.push_back(s1);
  signatures.push_back(s2);

  opcuabridge::Signed s = tutils::CreateSigned(5);

  opcuabridge::CurrentTime ct;
  ct.setSignatures(signatures);
  ct.setSigned(s);
  retval = ct.ClientWrite(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  // write metadata files
  int guid = time(NULL);
  opcuabridge::MetadataFiles mds;
  mds.setGUID(guid);
  mds.setNumberOfMetadataFiles(1);
  retval = mds.ClientWrite(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  std::string image_filename, image_hash;
  std::size_t image_size;
  EXPECT_TRUE(fs::exists(kMetadataFilePath));
  {
    std::ifstream mdfile(kMetadataFilePath.c_str());
    mdfile >> image_filename;
    mdfile >> image_size;
    mdfile >> image_hash;
  }

  opcuabridge::MetadataFile md;
  md.setGUID(guid);
  md.setFileNumber(1);
  md.setFilename(kMetadataFileName);
  std::vector<unsigned char> metadata;
  std::ifstream mdfile(kMetadataFilePath.c_str(), std::ios::binary);
  std::copy(std::istreambuf_iterator<char>(mdfile), std::istreambuf_iterator<char>(), std::back_inserter(metadata));
  md.setMetadata(metadata);
  retval = md.ClientWrite(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  // check image request
  opcuabridge::ImageRequest ir;
  retval = ir.ClientRead(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);
  EXPECT_EQ(kUpdateImageFileName, ir.getFilename());

  // write image
  EXPECT_TRUE(fs::exists(kImageFilePath));

  opcuabridge::ImageFile img_file;
  img_file.setFilename(kUpdateImageFileName);
  img_file.setNumberOfBlocks(kUpdateImageSize / kUpdateImageBlockSize);
  img_file.setBlockSize(kUpdateImageBlockSize);
  retval = img_file.ClientWrite(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  opcuabridge::ImageBlock img_block;
  img_block.setFilename(image_filename);
  std::vector<unsigned char> block(kUpdateImageBlockSize);
  std::ifstream image_file(kImageFilePath.c_str(), std::ios::binary);
  for (std::size_t b = 1; b <= img_file.getNumberOfBlocks(); ++b) {
    image_file.read(reinterpret_cast<char*>(&block[0]), kUpdateImageBlockSize);
    img_block.setBlockNumber(b);
    img_block.setBlock(block);
    retval = img_block.ClientWrite(client);
    EXPECT_EQ(retval, UA_STATUSCODE_GOOD);
  }

  // read version report after update
  opcuabridge::VersionReport updated_vr;
  retval = updated_vr.ClientRead(client);
  EXPECT_EQ(retval, UA_STATUSCODE_GOOD);

  opcuabridge::Image installed_image =
      updated_vr.getEcuVersionManifest().getEcuVersionManifestSigned().getInstalledImage();
  EXPECT_EQ(installed_image.getFilename(), kUpdateImageFileName);
  EXPECT_EQ(installed_image.getLength(), kUpdateImageSize);
  EXPECT_EQ(installed_image.getHashes()[0].getDigest(), image_hash);

  UA_Client_disconnect(client);
  UA_Client_delete(client);
}

#ifndef __NO_MAIN__
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
