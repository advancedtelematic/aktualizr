#include "filelist.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <cstring>

namespace fs = boost::filesystem;

namespace opcuabridge {

FileSetEntryHash::result_type FileSetEntryHash::operator()(FileSetEntryHash::argument_type const& e) const {
  result_type seed = 0;
  argument_type x = e;
  for (; *x != '\0'; ++x) {
    boost::hash_combine(seed, *x);
  }
  return seed;
}

bool FileSetEntryEqual::operator()(const FileSetEntryEqual::argument_type& lhs,
                                   const FileSetEntryEqual::argument_type& rhs) const {
  return (0 == strcmp(reinterpret_cast<const char*>(lhs), reinterpret_cast<const char*>(rhs)));
}

std::size_t UpdateFileList(FileList* filelist, const fs::path& repo_dir_path) {
  FileList::block_type& block = filelist->getBlock();
  std::size_t file_count = 0;
  fs::recursive_directory_iterator repo_dir_it_end, repo_dir_it(repo_dir_path);
  for (; repo_dir_it != repo_dir_it_end; ++repo_dir_it) {
    const fs::path& ent_path = repo_dir_it->path();
    if (fs::is_regular_file(ent_path)) {
      fs::path rel_path = fs::relative(ent_path, repo_dir_path);
      std::copy(rel_path.native().begin(), rel_path.native().end(), std::back_inserter(block));
      block.push_back('\0');
      ++file_count;
    }
  }
  return file_count;
}

void UpdateFileUnorderedSet(FileUnorderedSet* file_unordered_set, const FileList& file_list) {
  if (!file_list.getBlock().empty()) {
    auto block_rev_it = file_list.getBlock().rbegin(), p = block_rev_it, block_rev_end_it = file_list.getBlock().rend();
    for (; block_rev_it != block_rev_end_it; p = block_rev_it) {
      if ((block_rev_end_it == ++block_rev_it) || ('\0' == *(block_rev_it))) {
        file_unordered_set->insert(&*p);
      }
    }
  }
}

}  // namespace opcuabridge
