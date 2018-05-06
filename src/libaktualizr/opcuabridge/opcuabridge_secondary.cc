#include <open62541.h>

#include "crypto/crypto.h"
#include "logging/logging.h"

#include "opcuabridge_test_utils.h"

#include <signal.h>
#include <cstdlib>
#include <fstream>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

UA_Boolean running = true;
TemporaryDirectory temp_dir("opcuabridge-secondary");

static void stopHandler(int sign) {
  LOG_INFO << "received ctrl-c";
  running = false;
}

fs::path metadata_filepath;

opcuabridge::Image installed_image;

BOOST_PP_LIST_FOR_EACH(DEFINE_MESSAGE, _, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

void OnBeforeReadVersionReport(opcuabridge::VersionReport* version_report) {
  opcuabridge::ECUVersionManifestSigned ecu_version_manifest_signed;
  ecu_version_manifest_signed.setInstalledImage(installed_image);

  opcuabridge::ECUVersionManifest ecu_version_manifest;
  ecu_version_manifest.setEcuVersionManifestSigned(ecu_version_manifest_signed);

  version_report->setEcuVersionManifest(ecu_version_manifest);
}

void OnAfterWriteMetadataFile(opcuabridge::MetadataFile* metadata_file) {
  metadata_filepath = temp_dir / metadata_file->getFilename();
  std::ofstream ofs(metadata_filepath.c_str(), std::ios::binary | std::ios::trunc);
  std::copy(metadata_file->getMetadata().begin(), metadata_file->getMetadata().end(),
            std::ostreambuf_iterator<char>(ofs.rdbuf()));
}

void OnBeforeReadImageRequest(opcuabridge::ImageRequest* image_request) {
  if (fs::exists(metadata_filepath)) {
    std::string image_filename;
    std::ifstream ifs(metadata_filepath.c_str());
    ifs >> image_filename;
    image_request->setFilename(image_filename);
  }
}

MultiPartSHA256Hasher hasher;
std::size_t image_blocks;

void OnAfterWriteImageFile(opcuabridge::ImageFile* image_file) {
  hasher = MultiPartSHA256Hasher();
  installed_image.setFilename(image_file->getFilename());
  installed_image.setLength(0);
  image_blocks = image_file->getNumberOfBlocks();
}

void OnAfterWriteImageBlock(opcuabridge::ImageBlock* image_block) {
  fs::path image_filepath = temp_dir / installed_image.getFilename();
  std::ofstream ofs(image_filepath.c_str(), std::ios::binary | std::ios::app);
  const std::vector<unsigned char>& block = image_block->getBlock();
  std::copy(block.begin(), block.end(), std::ostreambuf_iterator<char>(ofs.rdbuf()));
  hasher.update(&block[0], block.size());
  installed_image.setLength(installed_image.getLength() + block.size());
  if (--image_blocks == 0) {
    std::vector<opcuabridge::Hash> hashes(1);
    hashes[0].setFunction(opcuabridge::HASH_FUN_SHA256);
    hashes[0].setDigest(hasher.getHexDigest());
    installed_image.setHashes(hashes);
  }
}

int main(int argc, char* argv[]) {
  UA_UInt16 port = (argc < 2 ? 4840 : (UA_UInt16)std::atoi(argv[1]));

  logger_init();

  LOG_INFO << "server port number: " << argv[1];

  struct sigaction sa;
  sa.sa_handler = &stopHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  UA_ServerConfig* config = UA_ServerConfig_new_minimal(port, NULL);
  config->logger = &opcuabridge_test_utils::BoostLogSecondary;

  UA_Server* server = UA_Server_new(config);

  BOOST_PP_LIST_FOR_EACH(INIT_SERVER_NODESET, server, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  vr.setOnBeforeReadCallback(&OnBeforeReadVersionReport);
  md.setOnAfterWriteCallback(&OnAfterWriteMetadataFile);
  ir.setOnBeforeReadCallback(&OnBeforeReadImageRequest);
  img_file.setOnAfterWriteCallback(&OnAfterWriteImageFile);
  img_block.setOnAfterWriteCallback(&OnAfterWriteImageBlock);

  UA_StatusCode retval = UA_Server_run(server, &running);
  UA_Server_delete(server);
  UA_ServerConfig_delete(config);

  return retval;
}
