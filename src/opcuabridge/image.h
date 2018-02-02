#ifndef OPCUABRIDGE_IMAGE_H_
#define OPCUABRIDGE_IMAGE_H_

#include "hash.h"

#include "common.h"

namespace opcuabridge {
class Image {
 public:
  Image() {}
  virtual ~Image() {}
 public:
  const std::string& get_filename() const { return filename_; }
  void set_filename(const std::string& filename) { filename_ = filename; }
  const std::size_t& get_length() const { return length_; }
  void set_length(const std::size_t& length) { length_ = length; }
  const std::size_t& get_numberOfHashes() const { return numberOfHashes_; }
  void set_numberOfHashes(const std::size_t& numberOfHashes) { numberOfHashes_ = numberOfHashes; }
  const std::vector<Hash>& get_hashes() const { return hashes_; }
  void set_hashes(const std::vector<Hash>& hashes) { hashes_ = hashes; }
 protected:
  std::string filename_;
  std::size_t length_;
  std::size_t numberOfHashes_;
  std::vector<Hash> hashes_;
 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = get_filename();
    v["length"] = static_cast<Json::Value::UInt>(get_length());
    v["numberOfHashes"] = static_cast<Json::Value::UInt>(get_numberOfHashes());
    v["hashes"] = converto::jsonArray(get_hashes());
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_filename(v["filename"].asString());
    set_length(v["length"].asUInt());
    set_numberOfHashes(v["numberOfHashes"].asUInt());
    set_hashes(converto::stdVector<Hash>(v["hashes"]));
  }
 private:
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "filename_", filename_);
    SERIALIZE_FIELD(ar, "length_", length_);
    SERIALIZE_FIELD(ar, "numberOfHashes_", numberOfHashes_);
    SERIALIZE_FIELD(ar, "hashes_", hashes_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGE_H_
