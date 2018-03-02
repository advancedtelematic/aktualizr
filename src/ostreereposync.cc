#include "logging.h"

#include <ostree.h>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace ostree_repo_sync {

bool CreateTempArchiveRepo(const fs::path& tmp_repo_dir) {
  GError* error = NULL;
  g_autoptr(GFile) tmp_repo_path = g_file_new_for_path(tmp_repo_dir.c_str());
  g_autoptr(OstreeRepo) tmp_repo = ostree_repo_new(tmp_repo_path);
  gboolean ost_ret_val = ostree_repo_create(tmp_repo,
                                            OSTREE_REPO_MODE_ARCHIVE,
                                            NULL, &error);
  if (!ost_ret_val) {
    LOG_ERROR << "OSTree sync error: unable to create temporary repo" << std::endl;
    return false;
  }
  return true;
}

bool LocalPullRepo(const fs::path& src_repo_dir, const fs::path& dst_repo_dir) {
  GError* error = NULL;
  gboolean ost_ret_val;

  // check source repo
  g_autoptr(GFile) src_repo_path = g_file_new_for_path(src_repo_dir.c_str());
  g_autoptr(OstreeRepo) src_repo = ostree_repo_new(src_repo_path);
  if (!ostree_repo_open(src_repo, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to open source repo";
    return false;
  }

  // check destination repo
  g_autoptr(GFile) dst_repo_path = g_file_new_for_path(dst_repo_dir.c_str());
  g_autoptr(OstreeRepo) dst_repo = ostree_repo_new(dst_repo_path);
  if (!ostree_repo_open(dst_repo, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to open destination repo";
    return false;
  }

  // collect refs to pull
  g_autoptr(GHashTable) refs_to_pull = NULL;
  g_autoptr(GPtrArray) refs_to_fetch = g_ptr_array_new_with_free_func(g_free);

  // Under some circumstances the following call may not be enough,
  // see the comment before the same call in ostree sources
  // (src/ostree/ot-builtin-pull-local.c).
  if (!ostree_repo_list_refs(src_repo, NULL, &refs_to_pull, NULL, &error)) {
    LOG_ERROR << "OSTree sync error: unable to get refs on source repo";
    return false;
  }
  {
    GHashTableIter hashiter;
    gpointer hkey, hvalue;

    g_hash_table_iter_init(&hashiter, refs_to_pull);
    while (g_hash_table_iter_next(&hashiter, &hkey, &hvalue))
      g_ptr_array_add(refs_to_fetch, g_strdup((const char*)hkey));
  }
  g_ptr_array_add(refs_to_fetch, NULL);

  // pull from source repo
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{s@v}", "flags",
                        g_variant_new_variant(g_variant_new_int32(0)));
  g_variant_builder_add(&builder, "{s@v}", "refs",
                        g_variant_new_variant(g_variant_new_strv((const char* const*)refs_to_fetch->pdata, -1)));
  GVariant* options = g_variant_builder_end(&builder);

  std::string src_repo_url("file://");
  src_repo_url += src_repo_dir.native();
  ost_ret_val = ostree_repo_pull_with_options(dst_repo, src_repo_url.c_str(),
                                              options, NULL, NULL, &error);
  g_variant_unref(options);
  if (!ost_ret_val) {
    LOG_ERROR << "OSTree sync error: unable to pull repository, "
              << error->message;
    return false;
  }
  return true;
}

} // namespace ostree_repo_sync
