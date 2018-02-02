#ifndef OPCUABRIDGE_IMAGEFILE_H_
#define OPCUABRIDGE_IMAGEFILE_H_

#include "common.h"

namespace opcuabridge {
class ImageFile {
 public:
  ImageFile() {}
  virtual ~ImageFile() {}
 public:
  const std::string& get_filename() const { return filename_; }
  void set_filename(const std::string& filename) { filename_ = filename; }
  const std::size_t& get_numberOfBlocks() const { return numberOfBlocks_; }
  void set_numberOfBlocks(const std::size_t& numberOfBlocks) { numberOfBlocks_ = numberOfBlocks; }
  const std::size_t& get_blockSize() const { return blockSize_; }
  void set_blockSize(const std::size_t& blockSize) { blockSize_ = blockSize; }
  INITSERVERNODESET_FUNCTION_DEFINITION(ImageFile)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                  // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                 // ClientWrite(UA_Client*)
 protected:
  std::string filename_;
  std::size_t numberOfBlocks_;
  std::size_t blockSize_;
 private:
  static const char* node_id_;
 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = get_filename();
    v["numberOfBlocks"] = static_cast<Json::Value::UInt>(get_numberOfBlocks());
    v["blockSize"] = static_cast<Json::Value::UInt>(get_blockSize());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_filename(v["filename"].asString());
    set_numberOfBlocks(v["numberOfBlocks"].asUInt());
    set_blockSize(v["blockSize"].asUInt());
  }
 private:
  WRAPMESSAGE_FUCTION_DEFINITION(ImageFile)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageFile)
  READ_FUNCTION_FRIEND_DECLARATION(ImageFile)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageFile)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageFile)
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "filename_", filename_);
    SERIALIZE_FIELD(ar, "numberOfBlocks_", numberOfBlocks_);
    SERIALIZE_FIELD(ar, "blockSize_", blockSize_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGEFILE_H_
