#include <open62541.h>

#include "logging/logging.h"
#include "opcuabridge/opcuabridge.h"
#include "package_manager/ostreemanager.h"
#include "package_manager/ostreereposync.h"

#include "opcuabridge_test_utils.h"

#include <iostream>
#include <fstream>
#include <signal.h>

#include <boost/filesystem.hpp>


namespace fs = boost::filesystem;

UA_Boolean running = true;

static void stopHandler(int) {
  LOG_INFO << "received ctrl-c";
  running = false;
}

int main(int argc, char* argv[]) {

  logger_init();

  UA_UInt16 port = (argc < 3 ? 4840 : (UA_UInt16)std::atoi(argv[2]));

  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " /path/to/repo [port]" << std::endl;
    return 0;
  }

  fs::path src_repo_dir(argv[1]);
  fs::path working_repo_dir;

  TemporaryDirectory temp_dir("opcuabridge-ostree-repo-sync-server");

  if (ostree_repo_sync::ArchiveModeRepo(src_repo_dir)) {
    LOG_INFO << "Source repo is in 'archive' mode - use it directly";
    working_repo_dir = src_repo_dir;
  } else {
    working_repo_dir = temp_dir.Path();

    if (!ostree_repo_sync::LocalPullRepo(src_repo_dir, working_repo_dir)) {
      LOG_ERROR << "OSTree: local pull to a temp repo is failed";
    }
  }

  // init server
  struct sigaction sa;
  sa.sa_handler = &stopHandler;
  sa.sa_flags   = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT  , &sa, NULL);
  sigaction(SIGTERM , &sa, NULL);

  UA_ServerConfig *config = UA_ServerConfig_new_minimal(port, NULL);
  config->logger = &opcuabridge_test_utils::BoostLogServer;

  UA_Server *server = UA_Server_new(config);

  // expose ostree repo files
  opcuabridge::FileList file_list;
  opcuabridge::FileData file_data(working_repo_dir);

  opcuabridge::UpdateFileList(&file_list, working_repo_dir);

  file_list.InitServerNodeset(server);
  file_data.InitServerNodeset(server);

  file_list.setOnAfterWriteCallback([&src_repo_dir, &working_repo_dir]
    (opcuabridge::FileList* file_list) {
      if (!file_list->getBlock().empty() && file_list->getBlock()[0] == '\0')
        if (!ostree_repo_sync::ArchiveModeRepo(src_repo_dir))
          if (!ostree_repo_sync::LocalPullRepo(working_repo_dir, src_repo_dir))
            LOG_ERROR << "OSTree: local pull to the source repo is failed";
    });

  // run server
  UA_StatusCode retval = UA_Server_run(server, &running);
  UA_Server_delete(server);
  UA_ServerConfig_delete(config);

  return retval;
}
