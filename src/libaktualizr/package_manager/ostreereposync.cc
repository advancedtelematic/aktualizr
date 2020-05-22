#include <ostree.h>

#include <cstring>

#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>
#include "logging/logging.h"

namespace {
const OstreeRepoMode kOstreeRepoModeArchive =
#if !defined(OSTREE_CHECK_VERSION)
    OSTREE_REPO_MODE_ARCHIVE_Z2;
#elif (OSTREE_CHECK_VERSION(2017, 12))
    OSTREE_REPO_MODE_ARCHIVE;
#else
    OSTREE_REPO_MODE_ARCHIVE_Z2;
#endif
}  // namespace

namespace fs = boost::filesystem;

namespace ostree_repo_sync {

bool ArchiveModeRepo(const fs::path& repo_dir) {
  GError* error = nullptr;
  GFile* repo_path = nullptr;
  OstreeRepo* repo = nullptr;

  BOOST_SCOPE_EXIT(&error, &repo_path, &repo) {  // NOLINT
    if (error != nullptr) {
      g_error_free(error);
    }
    if (repo_path != nullptr) {
      g_object_unref(repo_path);
    }
    if (repo != nullptr) {
      g_object_unref(repo);
    }
  }
  BOOST_SCOPE_EXIT_END

  repo_path = g_file_new_for_path(repo_dir.c_str());
  repo = ostree_repo_new(repo_path);

  gboolean open_succeed = ostree_repo_open(repo, nullptr, &error);

  return ((open_succeed != 0) && (ostree_repo_get_mode(repo) == kOstreeRepoModeArchive));
}

bool LocalPullRepo(const fs::path& src_repo_dir, const fs::path& dst_repo_dir, const std::string& ref_hash) {
  GError* error = nullptr;
  GVariant* options = nullptr;
  GHashTable* refs = nullptr;
  GPtrArray* refs_to_fetch = g_ptr_array_new_with_free_func(g_free);
  OstreeRepo *src_repo = nullptr, *dst_repo = nullptr;
  GFile *src_repo_path = nullptr, *dst_repo_path = nullptr;

  BOOST_SCOPE_EXIT(&error, &options, &refs, &refs_to_fetch, &src_repo_path, &src_repo, &dst_repo_path,  // NOLINT
                   &dst_repo) {
    if (error != nullptr) {
      g_error_free(error);
    }
    if (options != nullptr) {
      g_variant_unref(options);
    }
    if (src_repo_path != nullptr) {
      g_object_unref(src_repo_path);
    }
    if (src_repo != nullptr) {
      g_object_unref(src_repo);
    }
    if (dst_repo_path != nullptr) {
      g_object_unref(dst_repo_path);
    }
    if (dst_repo != nullptr) {
      g_object_unref(dst_repo);
    }
    if (refs != nullptr) {
      g_hash_table_unref(refs);
    }
    g_ptr_array_unref(refs_to_fetch);
  }
  BOOST_SCOPE_EXIT_END

  // check source repo
  src_repo_path = g_file_new_for_path(src_repo_dir.c_str());
  src_repo = ostree_repo_new(src_repo_path);
  if (ostree_repo_open(src_repo, nullptr, &error) == 0) {
    LOG_ERROR << "OSTree sync error: unable to open source repo, " << error->message;
    return false;
  }

  // check destination repo
  dst_repo_path = g_file_new_for_path(dst_repo_dir.c_str());
  dst_repo = ostree_repo_new(dst_repo_path);
  if (ostree_repo_create(dst_repo, kOstreeRepoModeArchive, nullptr, &error) == 0) {
    LOG_ERROR << "OSTree sync error: unable to open destination repo, " << error->message;
    return false;
  }

  // collect refs to pull
  //
  // Under some circumstances the following call may not be enough,
  // see the comment before the same call in OSTree sources
  // (src/ostree/ot-builtin-pull-local.c).
  if (ostree_repo_list_refs(src_repo, nullptr, &refs, nullptr, &error) == 0) {
    LOG_ERROR << "OSTree sync error: unable to get refs on source repo, " << error->message;
    return false;
  }
  {
    GHashTableIter hashiter;
    gpointer hkey, hvalue;

    g_hash_table_iter_init(&hashiter, refs);
    while (g_hash_table_iter_next(&hashiter, &hkey, &hvalue) != 0) {
      g_ptr_array_add(refs_to_fetch, g_strdup(static_cast<const char*>(hkey)));
    }
  }
  g_ptr_array_add(refs_to_fetch, nullptr);

  // pull from source repo
  const char* const refs_to_fetch_list[] = {ref_hash.c_str()};
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(0)));
  if (strlen(refs_to_fetch_list[0]) == 0) {
    g_variant_builder_add(
        &builder, "{s@v}", "refs",
        g_variant_new_variant(g_variant_new_strv(reinterpret_cast<const char* const*>(refs_to_fetch->pdata), -1)));
  } else {
    g_variant_builder_add(&builder, "{s@v}", "refs", g_variant_new_variant(g_variant_new_strv(refs_to_fetch_list, 1)));
  }
  options = g_variant_ref_sink(g_variant_builder_end(&builder));

  std::string src_repo_url("file://");
  src_repo_url += src_repo_dir.native();
  if (ostree_repo_pull_with_options(dst_repo, src_repo_url.c_str(), options, nullptr, nullptr, &error) == 0) {
    LOG_ERROR << "OSTree sync error: unable to pull repository, " << error->message;
    return false;
  }
  return true;
}

fs::path GetOstreeRepoPath(const fs::path& ostree_sysroot_path) { return (ostree_sysroot_path / "ostree" / "repo"); }

}  // namespace ostree_repo_sync
