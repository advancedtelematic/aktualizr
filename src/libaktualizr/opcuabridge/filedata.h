#ifndef OPCUABRIDGE_FILEDATA_H_
#define OPCUABRIDGE_FILEDATA_H_

#include "common.h"

#include <boost/filesystem.hpp>
#include <utility>

namespace opcuabridge {
class FileData : public MessageFileData {
 public:
  FileData() = default;
  explicit FileData(boost::filesystem::path base_path) : base_path_(std::move(base_path)) {}
  virtual ~FileData() = default;

  const boost::filesystem::path& getBasePath() const { return base_path_; }
  void setBasePath(const boost::filesystem::path& base_path) { base_path_ = base_path; }
  const boost::filesystem::path& getFilePath() const { return file_path_; }
  void setFilePath(const boost::filesystem::path& file_path) { file_path_ = file_path; }

  std::string getFullFilePath() const override { return (getBasePath() / getFilePath()).native(); }

  INITSERVERNODESET_FILE_FUNCTION_DEFINITION(FileData)  // InitServerNodeset(UA_Server*)
  CLIENTWRITE_FILE_FUNCTION_DEFINITION()                // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<FileData>::type cb) { on_before_read_cb_ = std::move(cb); }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<FileData>::type cb) { on_after_write_cb_ = std::move(cb); }

 protected:
  boost::filesystem::path base_path_;
  boost::filesystem::path file_path_;

  MessageOnBeforeReadCallback<FileData>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<FileData>::type on_after_write_cb_;

 private:
  static const char* node_id_;
  static const char* bin_node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = getFilePath().native();
    return v;
  }
  void unwrapMessage(Json::Value v) { setFilePath(v["filename"].asString()); }

  WRAPMESSAGE_FUCTION_DEFINITION(FileData)
  UNWRAPMESSAGE_FUCTION_DEFINITION(FileData)
  READ_FUNCTION_FRIEND_DECLARATION(FileData)
  WRITE_FUNCTION_FRIEND_DECLARATION(FileData)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(FileData)
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_FILEDATA_H_
