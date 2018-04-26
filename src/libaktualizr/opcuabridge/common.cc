#include "configuration.h"
#include "currenttime.h"
#include "ecuversionmanifest.h"
#include "ecuversionmanifestsigned.h"
#include "filedata.h"
#include "filelist.h"
#include "hash.h"
#include "image.h"
#include "imageblock.h"
#include "imagefile.h"
#include "imagerequest.h"
#include "metadatafile.h"
#include "metadatafiles.h"
#include "originalmanifest.h"
#include "signature.h"
#include "signed.h"
#include "versionreport.h"

#include "logging/logging.h"

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <fstream>

namespace fs = boost::filesystem;

namespace opcuabridge {
const UA_UInt16 kNSindex = 1;
const char *kLocale = "en-US";

const char *VersionReport::node_id_ = "VersionReport";
const char *Configuration::node_id_ = "Configuration";
const char *CurrentTime::node_id_ = "CurrentTime";
const char *MetadataFiles::node_id_ = "MetadataFiles";
const char *MetadataFile::node_id_ = "MetadataFile";
const char *MetadataFile::bin_node_id_ = "MetadataFile_BinaryData";
const char *ImageRequest::node_id_ = "ImageRequest";
const char *ImageFile::node_id_ = "ImageFile";
const char *ImageBlock::node_id_ = "ImageBlock";
const char *ImageBlock::bin_node_id_ = "ImageBlock_BinaryData";
const char *FileData::node_id_ = "FileData";
const char *FileData::bin_node_id_ = "FileData_BinaryData";
const char *FileList::node_id_ = "FileList";
const char *FileList::bin_node_id_ = "FileList_BinaryData";
const char *OriginalManifest::node_id_ = "OriginalManifest";
const char *OriginalManifest::bin_node_id_ = "OriginalManifest_BinaryData";
}  // namespace opcuabridge

namespace opcuabridge {

const std::size_t kLogMsgBuffSize = 256;

void BoostLogOpcua(UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args) {
  char msg_buff[kLogMsgBuffSize];
  vsnprintf(msg_buff, kLogMsgBuffSize, msg, args);
  BOOST_LOG_STREAM_WITH_PARAMS(
      boost::log::trivial::logger::get(),
      (boost::log::keywords::severity = static_cast<boost::log::trivial::severity_level>(level)))
      << "OPC-UA " << msg_buff;
}

template <>
UA_StatusCode read<MessageBinaryData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                      const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                      const UA_NumericRange *range, UA_DataValue *dataValue) {
  const BinaryDataType &bin_data = *static_cast<BinaryDataType *>(nodeContext);

  UA_Variant_setArrayCopy(&dataValue->value, &bin_data[0], bin_data.size(), &UA_TYPES[UA_TYPES_BYTE]);
  dataValue->hasValue = !bin_data.empty();

  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode write<MessageBinaryData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                       const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                       const UA_DataValue *data) {
  if (!UA_Variant_isEmpty(&data->value) && UA_Variant_hasArrayType(&data->value, &UA_TYPES[UA_TYPES_BYTE])) {
    auto *bin_data = static_cast<BinaryDataType *>(nodeContext);
    bin_data->resize(data->value.arrayLength);
    const auto *src = static_cast<const unsigned char *>(data->value.data);
    std::copy(src, src + data->value.arrayLength, bin_data->begin());
  }
  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode read<MessageFileData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                    const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                    const UA_NumericRange *range, UA_DataValue *dataValue) {
  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode write<MessageFileData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                     const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                     const UA_DataValue *data) {
  if (!UA_Variant_isEmpty(&data->value) && UA_Variant_hasArrayType(&data->value, &UA_TYPES[UA_TYPES_BYTE])) {
    auto *mfd = static_cast<MessageFileData *>(nodeContext);

    fs::path full_file_path = mfd->getFullFilePath();
    if (!fs::exists(full_file_path)) {
      fs::create_directories(full_file_path.parent_path());
    }
    std::ofstream ofs(full_file_path.c_str(), std::ios::binary | std::ios::app);
    if (ofs) {
      ofs.write(static_cast<const char *>(data->value.data), data->value.arrayLength);
      if (!ofs) {
        LOG_ERROR << "File write error: " << full_file_path.native();
      }
    } else {
      LOG_ERROR << "File open error: " << full_file_path.native();
    }
  }
  return UA_STATUSCODE_GOOD;
}

namespace internal {

UA_StatusCode ClientWriteFile(UA_Client *client, const char *node_id, const boost::filesystem::path &file_path,
                              const std::size_t block_size) {
  UA_StatusCode retval;

  boost::iostreams::mapped_file_source file(file_path.native());
  boost::iostreams::mapped_file_source::iterator f_it = file.begin();

  const std::size_t total_size = file.size();
  std::size_t written_size = 0;
  UA_Variant *val = UA_Variant_new();
  while (f_it != file.end()) {
    std::size_t current_block_size = std::min(block_size, (total_size - written_size));
    UA_Variant_setArray(val, const_cast<char *>(&(*f_it)), current_block_size, &UA_TYPES[UA_TYPES_BYTE]);
    val->storageType = UA_VARIANT_DATA_NODELETE;
    retval = UA_Client_writeValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
    if (retval != UA_STATUSCODE_GOOD) {
      return retval;
    }
    std::advance(f_it, current_block_size);
    written_size += current_block_size;
  }
  UA_Variant_delete(val);

  return UA_STATUSCODE_GOOD;
}

}  // namespace internal

}  // namespace opcuabridge
