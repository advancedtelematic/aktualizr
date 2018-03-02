#include <open62541.h>

#include <opcuabridge/opcuabridge.h>
#include <ostreereposync.h>

#include <iostream>
#include <fstream>
#include <signal.h>

#include <boost/filesystem.hpp>

#include <ostree.h>

namespace fs = boost::filesystem;

UA_Boolean running      = true;
UA_Logger logger        = UA_Log_Stdout;

static void stopHandler(int sign) {
  UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "received ctrl-c");
  running = false;
}

int main(int argc, char* argv[]) {

  GError* error = NULL;

  if (argc < 2)
    return 0;

  fs::path root_repo_dir(argv[1]);

  // check source repo
  g_autoptr(GFile) root_repo_path = g_file_new_for_path(root_repo_dir.c_str());
  g_autoptr(OstreeRepo) root_repo = ostree_repo_new(root_repo_path);
  if (!ostree_repo_open(root_repo, NULL, &error)) {
    std::clog << "Error: unable to open source repo" << std::endl;
    return 1;
  }

  // init temporary ostree repo using archive format
  TemporaryDirectory temp_dir("opcuabridge-ostree-repo-sync-server");
  fs::path tmp_repo_dir(temp_dir.Path());

  if (   !ostree_repo_sync::CreateTempArchiveRepo(tmp_repo_dir)
      || !ostree_repo_sync::LocalPullRepo(root_repo_dir, tmp_repo_dir)) {
    std::clog << "OSTree: local pull to a temp repo is failed" << std::endl;
  }

  // init server
  struct sigaction sa;
  sa.sa_handler = &stopHandler;
  sa.sa_flags   = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT  , &sa, NULL);
  sigaction(SIGTERM , &sa, NULL);

  UA_ServerConfig *config = UA_ServerConfig_new_default();
  UA_Server *server = UA_Server_new(config);

  // expose ostree repo files
  opcuabridge::FileList file_list;
  opcuabridge::FileData file_data(tmp_repo_dir);

  opcuabridge::UpdateFileList(&file_list, tmp_repo_dir);

  file_list.InitServerNodeset(server);
  file_data.InitServerNodeset(server);

  file_list.setOnAfterWriteCallback([&root_repo_dir, &tmp_repo_dir]
    (opcuabridge::FileList* file_list) {
      if (!file_list->getBlock().empty() && file_list->getBlock()[0] == '\0') {
        if (!ostree_repo_sync::LocalPullRepo(tmp_repo_dir, root_repo_dir))
          std::clog << "OSTree: local pull to the source repo is failed"
                    << std::endl;
      }
    });

  // run server
  UA_StatusCode retval = UA_Server_run(server, &running);
  UA_Server_delete(server);
  UA_ServerConfig_delete(config);

  return retval;
}
