#ifndef OPCUABRIDGE_FILELIST_H_
#define OPCUABRIDGE_FILELIST_H_

#include "imageblock.h"

#include <unordered_set>
#include <utility>

namespace boost {
namespace filesystem {
class path;
}  // namespace filesystem
}  // namespace boost

namespace opcuabridge {

class FileList {
 public:
  typedef BinaryDataType block_type;

  FileList() = default;
  virtual ~FileList() = default;

  block_type& getBlock() { return block_; }
  const block_type& getBlock() const { return block_; }
  void setBlock(const block_type& block) { block_ = block; }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(FileList, &block_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&block_)                   // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&block_)                  // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<FileList>::type cb) { on_before_read_cb_ = std::move(cb); }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<FileList>::type cb) { on_after_write_cb_ = std::move(cb); }

 protected:
  block_type block_;

  MessageOnBeforeReadCallback<FileList>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<FileList>::type on_after_write_cb_;

 private:
  static const char* node_id_;
  static const char* bin_node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    return v;
  }
  void unwrapMessage(const Json::Value& v) {}

  WRAPMESSAGE_FUCTION_DEFINITION(FileList)
  UNWRAPMESSAGE_FUCTION_DEFINITION(FileList)
  READ_FUNCTION_FRIEND_DECLARATION(FileList)
  WRITE_FUNCTION_FRIEND_DECLARATION(FileList)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(FileList)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() { SERIALIZE_FIELD(ar, "block_", block_); }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};

typedef const FileList::block_type::value_type* FileSetEntry;

struct FileSetEntryHash {
  typedef FileSetEntry argument_type;
  typedef std::size_t result_type;

  result_type operator()(argument_type const& e) const;
};

struct FileSetEntryEqual {
  typedef FileSetEntry argument_type;

  bool operator()(const argument_type& lhs, const argument_type& rhs) const;
};

typedef std::unordered_set<FileSetEntry, FileSetEntryHash, FileSetEntryEqual> FileUnorderedSet;

std::size_t UpdateFileList(FileList* /*filelist*/, const boost::filesystem::path& /*repo_dir_path*/);
void UpdateFileUnorderedSet(FileUnorderedSet* /*file_unordered_set*/, const FileList& /*file_list*/);

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_FILELIST_H_
