#ifndef OSTREEREPOSYNC_H_
#define OSTREEREPOSYNC_H_

#include <string>

namespace boost {
namespace filesystem {
class path;
}
}  // namespace boost

namespace ostree_repo_sync {

bool ArchiveModeRepo(const boost::filesystem::path& tmp_repo_dir);
bool LocalPullRepo(const boost::filesystem::path& src_repo_dir, const boost::filesystem::path& dst_repo_dir,
                   const std::string& = "");
boost::filesystem::path GetOstreeRepoPath(const boost::filesystem::path& ostree_sysroot_path);

}  // namespace ostree_repo_sync

#endif  // OSTREEREPOSYNC_H_
