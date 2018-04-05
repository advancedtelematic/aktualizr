#include "logging.h"

#include <ostree.h>

#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>

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
  GError* error = NULL;
  GFile* repo_path = NULL;
  OstreeRepo* repo = NULL;

  BOOST_SCOPE_EXIT(&error, &repo_path, &repo) {
    if (error) g_error_free(error);
    if (repo_path) g_object_unref(repo_path);
    if (repo) g_object_unref(repo);
  }
  BOOST_SCOPE_EXIT_END

  repo_path = g_file_new_for_path(repo_dir.c_str());
  repo = ostree_repo_new(repo_path);

  gboolean open_succeed = ostree_repo_open(repo, NULL, &error);

  return (open_succeed && (ostree_repo_get_mode(repo) == kOstreeRepoModeArchive));
}

bool LocalPullRepo(const fs::path& src_repo_dir, const fs::path& dst_repo_dir) {
  GError* error = NULL;
  GVariant* options = NULL;
  GHashTable* refs = NULL;
  GPtrArray* refs_to_fetch = g_ptr_array_new_with_free_func(g_free);
  OstreeRepo *src_repo = NULL, *dst_repo = NULL;
  GFile *src_repo_path = NULL, *dst_repo_path = NULL;

  BOOST_SCOPE_EXIT(&error, &options, &refs, &refs_to_fetch, &src_repo_path, &src_repo, &dst_repo_path, &dst_repo) {
    if (error) g_error_free(error);
    if (options) g_variant_unref(options);
    if (src_repo_path) g_object_unref(src_repo_path);
    if (src_repo) g_object_unref(src_repo);
    if (dst_repo_path) g_object_unref(dst_repo_path);
    if (dst_repo) g_object_unref(dst_repo);
    if (refs) g_hash_table_unref(refs);
    g_ptr_array_unref(refs_to_fetch);
  }
  BOOST_SCOPE_EXIT_END

  // check source repo
  src_repo_path = g_file_new_for_path(src_repo_dir.c_str());
  src_repo = ostree_repo_new(src_repo_path);
  if (!ostree_repo_open(src_repo, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to open source repo, " << error->message;
    return false;
  }

  // check destination repo
  dst_repo_path = g_file_new_for_path(dst_repo_dir.c_str());
  dst_repo = ostree_repo_new(dst_repo_path);
  if (!ostree_repo_create(dst_repo, kOstreeRepoModeArchive, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to open destination repo, " << error->message;
    return false;
  }

  // collect refs to pull
  //
  // Under some circumstances the following call may not be enough,
  // see the comment before the same call in ostree sources
  // (src/ostree/ot-builtin-pull-local.c).
  if (!ostree_repo_list_refs(src_repo, NULL, &refs, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to get refs on source repo, " << error->message;
    return false;
  }
  {
    GHashTableIter hashiter;
    gpointer hkey, hvalue;

    g_hash_table_iter_init(&hashiter, refs);
    while (g_hash_table_iter_next(&hashiter, &hkey, &hvalue))
      g_ptr_array_add(refs_to_fetch, g_strdup((const char*)hkey));
  }
  g_ptr_array_add(refs_to_fetch, NULL);

  // pull from source repo
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(0)));
  g_variant_builder_add(&builder, "{s@v}", "refs",
                        g_variant_new_variant(g_variant_new_strv((const char* const*)refs_to_fetch->pdata, -1)));
  options = g_variant_ref_sink(g_variant_builder_end(&builder));

  std::string src_repo_url("file://");
  src_repo_url += src_repo_dir.native();
  if (!ostree_repo_pull_with_options(dst_repo, src_repo_url.c_str(), options, NULL, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to pull repository, " << error->message;
    return false;
  }
  return true;
}

}  // namespace ostree_repo_sync
