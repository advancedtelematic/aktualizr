#ifndef OPCUABRIDGE_METADATAFILE_H_
#define OPCUABRIDGE_METADATAFILE_H_

#include <utility>

#include "common.h"

namespace opcuabridge {
class MetadataFile {
 public:
  typedef BinaryDataType block_type;

  MetadataFile() = default;
  virtual ~MetadataFile() = default;

  const int& getGUID() const { return GUID_; }
  void setGUID(const int& GUID) { GUID_ = GUID; }
  const std::size_t& getFileNumber() const { return fileNumber_; }
  void setFileNumber(const std::size_t& fileNumber) { fileNumber_ = fileNumber; }
  const std::string& getFilename() const { return filename_; }
  void setFilename(const std::string& filename) { filename_ = filename; }
  block_type& getMetadata() { return metadata_; }
  const block_type& getMetadata() const { return metadata_; }
  void setMetadata(const block_type& metadata) { metadata_ = metadata; }
  void setMetadata(const Json::Value& metadata) {
    Json::FastWriter fw;
    std::string s = fw.write(metadata);
    metadata_.assign(s.begin(), s.end());
  }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(MetadataFile, &metadata_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&metadata_)                       // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&metadata_)                      // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<MetadataFile>::type cb) {
    on_before_read_cb_ = std::move(cb);
  }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<MetadataFile>::type cb) {
    on_after_write_cb_ = std::move(cb);
  }

 protected:
  int GUID_{};
  std::size_t fileNumber_{};
  std::string filename_;
  block_type metadata_;

  MessageOnBeforeReadCallback<MetadataFile>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<MetadataFile>::type on_after_write_cb_;

 private:
  static const char* node_id_;
  static const char* bin_node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["GUID"] = getGUID();
    v["fileNumber"] = static_cast<Json::Value::UInt>(getFileNumber());
    v["filename"] = getFilename();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setGUID(v["GUID"].asInt());
    setFileNumber(v["fileNumber"].asUInt());
    setFilename(v["filename"].asString());
  }

  WRAPMESSAGE_FUCTION_DEFINITION(MetadataFile)
  UNWRAPMESSAGE_FUCTION_DEFINITION(MetadataFile)
  READ_FUNCTION_FRIEND_DECLARATION(MetadataFile)
  WRITE_FUNCTION_FRIEND_DECLARATION(MetadataFile)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(MetadataFile)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "GUID_", GUID_);
    SERIALIZE_FIELD(ar, "fileNumber_", fileNumber_);
    SERIALIZE_FIELD(ar, "filename_", filename_);
    SERIALIZE_FIELD(ar, "metadata_", metadata_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_METADATAFILE_H_
