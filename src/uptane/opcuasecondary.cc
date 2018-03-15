#include "opcuasecondary.h"

#include <opcuabridge/opcuabridgeclient.h>
#include "secondaryconfig.h"

#include <logging.h>
#include <ostreereposync.h>

#include <algorithm>
#include <iterator>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

namespace fs = boost::filesystem;
namespace io = boost::iostreams;

namespace Uptane {

OpcuaSecondary::OpcuaSecondary(const SecondaryConfig& sconfig) : SecondaryInterface(sconfig) {}

OpcuaSecondary::~OpcuaSecondary() {}

std::string OpcuaSecondary::getSerial() {
  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  return client.recvConfiguration().getSerial();
}
std::string OpcuaSecondary::getHwId() {
  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  return client.recvConfiguration().getHwId();
}
std::string OpcuaSecondary::getPublicKey() {
  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  return client.recvConfiguration().getPublicKey();
}

Json::Value OpcuaSecondary::getManifest() {
  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  return client.recvVersionReport().getEcuVersionManifest().wrapMessage();
}

opcuabridge::MetadataFile makeMetadataFile(const Target& target);

bool OpcuaSecondary::putMetadata(const MetaPack& meta_pack) {
  const auto& director_targets = meta_pack.director_targets.targets;
  const auto& image_targets = meta_pack.image_targets.targets;

  std::vector<opcuabridge::MetadataFile> metadatafiles;
  metadatafiles.reserve(director_targets.size() + image_targets.size());

  std::transform(director_targets.begin(), director_targets.end(), std::back_inserter(metadatafiles), makeMetadataFile);
  std::transform(image_targets.begin(), image_targets.end(), std::back_inserter(metadatafiles), makeMetadataFile);

  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  return client.sendMetadataFiles(metadatafiles);
}

bool OpcuaSecondary::sendFirmware(const std::string& data) {
  const fs::path source_repo_dir_path(data);

  opcuabridge::Client client(opcuabridge::SelectEndPoint(SecondaryInterface::sconfig));
  if (!client) return false;

  bool retval = true;
  if (ostree_repo_sync::ArchiveModeRepo(source_repo_dir_path)) {
    retval = client.syncDirectoryFiles(source_repo_dir_path);
  } else {
    TemporaryDirectory temp_dir("opcuabridge-ostree-sync-working-repo");
    const fs::path working_repo_dir_path = temp_dir.Path();

    if (!ostree_repo_sync::LocalPullRepo(source_repo_dir_path, working_repo_dir_path)) {
      LOG_ERROR << "OSTree repo sync failed: unable to local pull from " << source_repo_dir_path.native();
      return false;
    }
    retval = client.syncDirectoryFiles(working_repo_dir_path);
  }
  return retval;
}

opcuabridge::MetadataFile makeMetadataFile(const Target& target) {
  opcuabridge::MetadataFile mf;
  mf.setFilename(target.filename());

  io::mapped_file_source file(mf.getFilename());
  std::copy(file.begin(), file.end(), std::back_inserter(mf.getMetadata()));

  return mf;
}

}  // namespace Uptane
