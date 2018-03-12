#ifndef OPCUABRIDGE_METADATAFILES_H_
#define OPCUABRIDGE_METADATAFILES_H_

#include "common.h"

namespace opcuabridge {
class MetadataFiles {
 public:
  MetadataFiles() {}
  virtual ~MetadataFiles() {}

  const int& getGUID() const { return GUID_; }
  void setGUID(const int& GUID) { GUID_ = GUID; }
  const std::size_t& getNumberOfMetadataFiles() const { return numberOfMetadataFiles_; }
  void setNumberOfMetadataFiles(const std::size_t& numberOfMetadataFiles) {
    numberOfMetadataFiles_ = numberOfMetadataFiles;
  }
  INITSERVERNODESET_FUNCTION_DEFINITION(MetadataFiles)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                      // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                     // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<MetadataFiles>::type cb) { on_before_read_cb_ = cb; }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<MetadataFiles>::type cb) { on_after_write_cb_ = cb; }

 protected:
  int GUID_;
  std::size_t numberOfMetadataFiles_;

  MessageOnBeforeReadCallback<MetadataFiles>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<MetadataFiles>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["GUID"] = getGUID();
    v["numberOfMetadataFiles"] = static_cast<Json::Value::UInt>(getNumberOfMetadataFiles());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setGUID(v["GUID"].asInt());
    setNumberOfMetadataFiles(v["numberOfMetadataFiles"].asUInt());
  }

  WRAPMESSAGE_FUCTION_DEFINITION(MetadataFiles)
  UNWRAPMESSAGE_FUCTION_DEFINITION(MetadataFiles)
  READ_FUNCTION_FRIEND_DECLARATION(MetadataFiles)
  WRITE_FUNCTION_FRIEND_DECLARATION(MetadataFiles)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(MetadataFiles)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "GUID_", GUID_);
    SERIALIZE_FIELD(ar, "numberOfMetadataFiles_", numberOfMetadataFiles_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_METADATAFILES_H_
