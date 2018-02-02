#ifndef OPCUABRIDGE_IMAGEBLOCK_H_
#define OPCUABRIDGE_IMAGEBLOCK_H_

#include "common.h"

namespace opcuabridge {
class ImageBlock {
 public:
  ImageBlock() {}
  virtual ~ImageBlock() {}
 public:
  const std::string& get_filename() const { return filename_; }
  void set_filename(const std::string& filename) { filename_ = filename; }
  const std::size_t& get_blockNumber() const { return blockNumber_; }
  void set_blockNumber(const std::size_t& blockNumber) { blockNumber_ = blockNumber; }
  const std::vector<unsigned char>& get_block() const { return block_; }
  void set_block(const std::vector<unsigned char>& block) { block_ = block; }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(ImageBlock, &block_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&block_)                     // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&block_)                    // ClientWrite(UA_Client*)
 protected:
  std::string filename_;
  std::size_t blockNumber_;
  std::vector<unsigned char> block_;
 private:
  static const char* node_id_;
  static const char* bin_node_id_;
 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = get_filename();
    v["blockNumber"] = static_cast<Json::Value::UInt>(get_blockNumber());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_filename(v["filename"].asString());
    set_blockNumber(v["blockNumber"].asUInt());
  }
 private:
  WRAPMESSAGE_FUCTION_DEFINITION(ImageBlock)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageBlock)
  READ_FUNCTION_FRIEND_DECLARATION(ImageBlock)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageBlock)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageBlock)
 private:
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
