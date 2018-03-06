#ifndef OSTREEREPOSYNC_H_
#define OSTREEREPOSYNC_H_

namespace boost { namespace filesystem { class path; } }

namespace ostree_repo_sync {

bool ArchiveModeRepo(const boost::filesystem::path& tmp_repo_dir);
bool LocalPullRepo(const boost::filesystem::path& src_repo_dir,
                   const boost::filesystem::path& dst_repo_dir);

} // namespace ostree_repo_sync

#endif//OSTREEREPOSYNC_H_
