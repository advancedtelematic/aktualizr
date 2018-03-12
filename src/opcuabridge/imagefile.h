#ifndef OPCUABRIDGE_IMAGEFILE_H_
#define OPCUABRIDGE_IMAGEFILE_H_

#include "common.h"

namespace opcuabridge {
class ImageFile {
 public:
  ImageFile() {}
  virtual ~ImageFile() {}

  const std::string& getFilename() const { return filename_; }
  void setFilename(const std::string& filename) { filename_ = filename; }
  const std::size_t& getNumberOfBlocks() const { return numberOfBlocks_; }
  void setNumberOfBlocks(const std::size_t& numberOfBlocks) { numberOfBlocks_ = numberOfBlocks; }
  const std::size_t& getBlockSize() const { return blockSize_; }
  void setBlockSize(const std::size_t& blockSize) { blockSize_ = blockSize; }
  INITSERVERNODESET_FUNCTION_DEFINITION(ImageFile)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                  // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                 // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<ImageFile>::type cb) { on_before_read_cb_ = cb; }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<ImageFile>::type cb) { on_after_write_cb_ = cb; }

 protected:
  std::string filename_;
  std::size_t numberOfBlocks_;
  std::size_t blockSize_;

  MessageOnBeforeReadCallback<ImageFile>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<ImageFile>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = getFilename();
    v["numberOfBlocks"] = static_cast<Json::Value::UInt>(getNumberOfBlocks());
    v["blockSize"] = static_cast<Json::Value::UInt>(getBlockSize());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setFilename(v["filename"].asString());
    setNumberOfBlocks(v["numberOfBlocks"].asUInt());
    setBlockSize(v["blockSize"].asUInt());
  }

  WRAPMESSAGE_FUCTION_DEFINITION(ImageFile)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageFile)
  READ_FUNCTION_FRIEND_DECLARATION(ImageFile)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageFile)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageFile)

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
