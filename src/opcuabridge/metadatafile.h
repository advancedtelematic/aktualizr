#ifndef OPCUABRIDGE_METADATAFILE_H_
#define OPCUABRIDGE_METADATAFILE_H_

#include "common.h"

namespace opcuabridge {
class MetadataFile {
 public:
  MetadataFile() {}
  virtual ~MetadataFile() {}
 public:
  const int& get_GUID() const { return GUID_; }
  void set_GUID(const int& GUID) { GUID_ = GUID; }
  const std::size_t& get_fileNumber() const { return fileNumber_; }
  void set_fileNumber(const std::size_t& fileNumber) { fileNumber_ = fileNumber; }
  const std::string& get_filename() const { return filename_; }
  void set_filename(const std::string& filename) { filename_ = filename; }
  const std::vector<unsigned char>& get_metadata() const { return metadata_; }
  void set_metadata(const std::vector<unsigned char>& metadata) { metadata_ = metadata; }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(MetadataFile, &metadata_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&metadata_)                       // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&metadata_)                      // ClientWrite(UA_Client*)
 protected:
  int GUID_;
  std::size_t fileNumber_;
  std::string filename_;
  std::vector<unsigned char> metadata_;
 private:
  static const char* node_id_;
  static const char* bin_node_id_;
 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["GUID"] = get_GUID();
    v["fileNumber"] = static_cast<Json::Value::UInt>(get_fileNumber());
    v["filename"] = get_filename();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_GUID(v["GUID"].asInt());
    set_fileNumber(v["fileNumber"].asUInt());
    set_filename(v["filename"].asString());
  }
 private:
  WRAPMESSAGE_FUCTION_DEFINITION(MetadataFile)
  UNWRAPMESSAGE_FUCTION_DEFINITION(MetadataFile)
  READ_FUNCTION_FRIEND_DECLARATION(MetadataFile)
  WRITE_FUNCTION_FRIEND_DECLARATION(MetadataFile)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(MetadataFile)
 private:
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
