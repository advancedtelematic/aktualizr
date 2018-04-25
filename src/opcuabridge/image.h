#ifndef OPCUABRIDGE_IMAGE_H_
#define OPCUABRIDGE_IMAGE_H_

#include "hash.h"

#include "common.h"

namespace opcuabridge {
class Image {
 public:
  Image() = default;
  virtual ~Image() = default;

  const std::string& getFilename() const { return filename_; }
  void setFilename(const std::string& filename) { filename_ = filename; }
  const std::size_t& getLength() const { return length_; }
  void setLength(const std::size_t& length) { length_ = length; }
  const std::vector<Hash>& getHashes() const { return hashes_; }
  void setHashes(const std::vector<Hash>& hashes) { hashes_ = hashes; }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["filepath"] = getFilename();
    v["fileinfo"]["length"] = static_cast<Json::Value::UInt>(getLength());
    v["fileinfo"]["hashes"] = convert_to::jsonArray(getHashes());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setFilename(v["filepath"].asString());
    setLength(v["fileinfo"]["length"].asUInt());
    setHashes(convert_to::stdVector<Hash>(v["fileinfo"]["hashes"]));
  }

 protected:
  std::string filename_;
  std::size_t length_{};
  std::vector<Hash> hashes_;

 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "filename_", filename_);
    SERIALIZE_FIELD(ar, "length_", length_);
    SERIALIZE_FIELD(ar, "hashes_", hashes_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGE_H_
