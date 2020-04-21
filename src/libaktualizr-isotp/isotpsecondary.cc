#include "isotpsecondary.h"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <array>
#include <future>

#include <boost/algorithm/hex.hpp>
#include <boost/lexical_cast.hpp>

#include "storage/invstorage.h"

#define LIBUPTINY_ISOTP_PRIMARY_CANID 0x7D8

constexpr size_t kChunkSize = 500;

enum class IsoTpUptaneMesType {
  kGetSerial = 0x01,
  kGetSerialResp = 0x41,
  kGetHwId = 0x02,
  kGetHwIdResp = 0x42,
  kGetPkey = 0x03,
  kGetPkeyResp = 0x43,
  kGetRootVer = 0x04,
  kGetRootVerResp = 0x44,
  kGetManifest = 0x05,
  kGetManifestResp = 0x45,
  kPutRoot = 0x06,
  kPutTargets = 0x07,
  kPutImageChunk = 0x08,
  kPutImageChunkAckErr = 0x48,
};

namespace Uptane {

IsoTpSecondary::IsoTpSecondary(const std::string& can_iface, uint16_t can_id, ImageReaderProvider image_reader_provider)
    : conn(can_iface, LIBUPTINY_ISOTP_PRIMARY_CANID, can_id),
      image_reader_provider_{std::move(image_reader_provider)} {}

EcuSerial IsoTpSecondary::getSerial() const {
  std::string out;
  std::string in;

  out += static_cast<char>(IsoTpUptaneMesType::kGetSerial);
  if (!conn.SendRecv(out, &in)) {
    return EcuSerial::Unknown();
  }

  if (in[0] != static_cast<char>(IsoTpUptaneMesType::kGetSerialResp)) {
    return EcuSerial::Unknown();
  }
  return EcuSerial(in.substr(1));
}

HardwareIdentifier IsoTpSecondary::getHwId() const {
  std::string out;
  std::string in;

  out += static_cast<char>(IsoTpUptaneMesType::kGetHwId);
  if (!conn.SendRecv(out, &in)) {
    return HardwareIdentifier::Unknown();
  }

  if (in[0] != static_cast<char>(IsoTpUptaneMesType::kGetHwIdResp)) {
    return HardwareIdentifier::Unknown();
  }
  return HardwareIdentifier(in.substr(1));
}

PublicKey IsoTpSecondary::getPublicKey() const {
  std::string out;
  std::string in;

  out += static_cast<char>(IsoTpUptaneMesType::kGetPkey);
  if (!conn.SendRecv(out, &in)) {
    return PublicKey("", KeyType::kUnknown);
  }

  if (in[0] != static_cast<char>(IsoTpUptaneMesType::kGetPkeyResp)) {
    return PublicKey("", KeyType::kUnknown);
  }
  return PublicKey(boost::algorithm::hex(in.substr(1)), KeyType::kED25519);
}

Uptane::Manifest IsoTpSecondary::getManifest() const {
  std::string out;
  std::string in;

  out += static_cast<char>(IsoTpUptaneMesType::kGetManifest);
  if (!conn.SendRecv(out, &in)) {
    return Json::Value(Json::nullValue);
  }

  if (in[0] != static_cast<char>(IsoTpUptaneMesType::kGetManifestResp)) {
    return Json::Value(Json::nullValue);
  }
  return Utils::parseJSON(in.substr(1));
}

int IsoTpSecondary::getRootVersion(bool director) const {
  if (!director) {
    return 0;
  }

  std::string out;
  std::string in;

  out += static_cast<char>(IsoTpUptaneMesType::kGetRootVer);
  if (!conn.SendRecv(out, &in)) {
    return -1;
  }

  if (in[0] != static_cast<char>(IsoTpUptaneMesType::kGetRootVerResp)) {
    return -1;
  }
  try {
    return boost::lexical_cast<int>(in.substr(1));
  } catch (boost::bad_lexical_cast const&) {
    return -1;
  }
}

bool IsoTpSecondary::putRoot(const std::string& root, bool director) {
  if (!director) {
    return true;
  }
  std::string out;
  out += static_cast<char>(IsoTpUptaneMesType::kPutRoot);
  out += root;

  return conn.Send(out);
}

bool IsoTpSecondary::putMetadata(const RawMetaPack& meta_pack) {
  std::string out;
  out += static_cast<char>(IsoTpUptaneMesType::kPutTargets);
  out += meta_pack.director_targets;

  return conn.Send(out);
}

data::ResultCode::Numeric IsoTpSecondary::sendFirmware(const Target& target) {
  (void)target;
  return data::ResultCode::Numeric::kOk;
}

data::ResultCode::Numeric IsoTpSecondary::install(const Target& target) {
  auto result = data::ResultCode::Numeric::kOk;

  try {
    std::unique_ptr<StorageTargetRHandle> image_reader = image_reader_provider_(target);

    auto image_size = image_reader->rsize();
    size_t num_chunks = (image_size / kChunkSize) + (static_cast<bool>(image_size % kChunkSize) ? 1 : 0);

    if (num_chunks > 127) {
      return data::ResultCode::Numeric::kInternalError;
    }

    for (size_t i = 0; i < num_chunks; ++i) {
      std::string out;
      std::string in;
      out += static_cast<char>(IsoTpUptaneMesType::kPutImageChunk);
      out += static_cast<char>(num_chunks);
      out += static_cast<char>(i + 1);

      std::array<char, kChunkSize> buf{};
      image_reader->rread(reinterpret_cast<uint8_t*>(buf.data()), kChunkSize);
      out += std::string(buf.data());

      if (!conn.SendRecv(out, &in)) {
        result = data::ResultCode::Numeric::kDownloadFailed;
        break;
      }
      if (in[0] != static_cast<char>(IsoTpUptaneMesType::kPutImageChunkAckErr)) {
        result = data::ResultCode::Numeric::kDownloadFailed;
        break;
      }
      if (in[1] != 0x00) {
        result = data::ResultCode::Numeric::kDownloadFailed;
        break;
      }
    }

  } catch (const std::exception& exc) {
    LOG_ERROR << "Failed to upload a target image: " << target.filename() << ", error " << exc.what();
    result = data::ResultCode::Numeric::kDownloadFailed;
  }
  return result;
}

}  // namespace Uptane
