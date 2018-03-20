#ifndef OPCUABRIDGE_FILELIST_H_
#define OPCUABRIDGE_FILELIST_H_

#include "imageblock.h"

#include <unordered_set>

namespace boost {
namespace filesystem {
class path;
}
}  // namespace boost

namespace opcuabridge {

typedef ImageBlock FileList;
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

std::size_t UpdateFileList(FileList*, const boost::filesystem::path&);
void UpdateFileUnorderedSet(FileUnorderedSet*, const FileList&);

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_FILELIST_H_
