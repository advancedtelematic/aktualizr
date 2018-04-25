#ifndef OPCUABRIDGE_IMAGEBLOCK_H_
#define OPCUABRIDGE_IMAGEBLOCK_H_

#include <utility>

#include "common.h"

namespace opcuabridge {
class ImageBlock {
 public:
  typedef BinaryDataType block_type;

  ImageBlock() = default;
  virtual ~ImageBlock() = default;

  const std::string& getFilename() const { return filename_; }
  void setFilename(const std::string& filename) { filename_ = filename; }
  const std::size_t& getBlockNumber() const { return blockNumber_; }
  void setBlockNumber(const std::size_t& blockNumber) { blockNumber_ = blockNumber; }
  block_type& getBlock() { return block_; }
  const block_type& getBlock() const { return block_; }
  void setBlock(const block_type& block) { block_ = block; }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(ImageBlock, &block_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&block_)                     // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&block_)                    // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<ImageBlock>::type cb) { on_before_read_cb_ = std::move(cb); }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<ImageBlock>::type cb) { on_after_write_cb_ = std::move(cb); }

 protected:
  std::string filename_;
  std::size_t blockNumber_{0};
  block_type block_;

  MessageOnBeforeReadCallback<ImageBlock>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<ImageBlock>::type on_after_write_cb_;

 private:
  static const char* node_id_;
  static const char* bin_node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = getFilename();
    v["blockNumber"] = static_cast<Json::Value::UInt>(getBlockNumber());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setFilename(v["filename"].asString());
    setBlockNumber(v["blockNumber"].asUInt());
  }

  WRAPMESSAGE_FUCTION_DEFINITION(ImageBlock)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageBlock)
  READ_FUNCTION_FRIEND_DECLARATION(ImageBlock)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageBlock)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageBlock)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "filename_", filename_);
    SERIALIZE_FIELD(ar, "blockNumber_", blockNumber_);
    SERIALIZE_FIELD(ar, "block_", block_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGEBLOCK_H_
