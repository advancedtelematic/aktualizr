#include "currenttime.h"
#include "ecuversionmanifest.h"
#include "ecuversionmanifestsigned.h"
#include "hash.h"
#include "image.h"
#include "imageblock.h"
#include "imagefile.h"
#include "imagerequest.h"
#include "metadatafile.h"
#include "metadatafiles.h"
#include "signature.h"
#include "signed.h"
#include "versionreport.h"

namespace opcuabridge {
const UA_UInt16 kNSindex                = 1;
const char* kLocale                     = "en-US";

const char* VersionReport::node_id_     = "VersionReport";
const char* CurrentTime::node_id_       = "CurrentTime";
const char* MetadataFiles::node_id_     = "MetadataFiles";
const char* MetadataFile::node_id_      = "MetadataFile";
const char* MetadataFile::bin_node_id_  = "MetadataFile_BinaryData";
const char* ImageRequest::node_id_      = "ImageRequest";
const char* ImageFile::node_id_         = "ImageFile";
const char* ImageBlock::node_id_        = "ImageBlock";
const char* ImageBlock::bin_node_id_    = "ImageBlock_BinaryData";
}  // namespace opcua_bridge

namespace opcuabridge {

template <>
UA_StatusCode read<BinaryData>(UA_Server *server, const UA_NodeId *sessionId,
                               void *sessionContext, const UA_NodeId *nodeId,
                               void *nodeContext, UA_Boolean sourceTimeStamp,
                               const UA_NumericRange *range, UA_DataValue *dataValue) {

  const BinaryDataContainer& bin_data =
    *static_cast<BinaryDataContainer*>(nodeContext);

  UA_Variant_setArrayCopy(&dataValue->value, &bin_data[0], bin_data.size(),
                          &UA_TYPES[UA_TYPES_BYTE]);
  dataValue->hasValue = !bin_data.empty();

  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode write<BinaryData>(UA_Server *server, const UA_NodeId *sessionId,
                                void *sessionContext, const UA_NodeId *nodeId,
                                void *nodeContext, const UA_NumericRange *range,
                                const UA_DataValue *data) {

  if (!UA_Variant_isEmpty(&data->value) &&
      UA_Variant_hasArrayType(&data->value, &UA_TYPES[UA_TYPES_BYTE])) {
    BinaryDataContainer* bin_data =
      static_cast<BinaryDataContainer*>(nodeContext);
    bin_data->resize(data->value.arrayLength);
    const unsigned char* src = 
      static_cast<const unsigned char *>(data->value.data);
    std::copy(src, src + data->value.arrayLength, bin_data->begin());
  }
  return UA_STATUSCODE_GOOD;
}

}  // namespace opcua_bridge
