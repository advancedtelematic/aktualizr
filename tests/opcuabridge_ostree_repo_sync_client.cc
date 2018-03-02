#include <open62541.h>

#include <opcuabridge/opcuabridge.h>
#include <ostreereposync.h>

#include <fstream>
#include <iostream>
#include <iterator>

#include <boost/filesystem.hpp>

#include "test_utils.h"

namespace fs = boost::filesystem;

int main(int argc, char* argv[]) {
  if (argc < 2)
    return 0;

  TemporaryDirectory temp_dir("opcuabridge-ostree-repo-sync-client");

  fs::path root_repo_dir(argv[1]);
  fs::path tmp_repo_dir(temp_dir.Path());

  if (   !ostree_repo_sync::CreateTempArchiveRepo(tmp_repo_dir)
      || !ostree_repo_sync::LocalPullRepo(root_repo_dir, tmp_repo_dir)) {
    std::clog << "OSTree: local pull to a temp repo is failed" << std::endl;
  }

  // init client
  UA_Client *client = UA_Client_new(UA_ClientConfig_default);

  UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://localhost:4840");
  if (retval != UA_STATUSCODE_GOOD) {
      UA_Client_delete(client);
      return (int)retval;
  }

  // sync repo files
  opcuabridge::FileList file_list;
  opcuabridge::FileData file_data(tmp_repo_dir);

  file_list.ClientRead(client);

  opcuabridge::FileUnorderedSet file_unordered_set;
  opcuabridge::UpdateFileUnorderedSet(&file_unordered_set, file_list);

  fs::recursive_directory_iterator
    repo_dir_it_end, repo_dir_it(tmp_repo_dir);
  for (; repo_dir_it != repo_dir_it_end; ++repo_dir_it) {
    const fs::path& ent_path = repo_dir_it->path();
    if (fs::is_regular_file(ent_path)) {
      fs::path rel_path = fs::relative(ent_path, tmp_repo_dir);
      if (file_unordered_set.count((opcuabridge::FileSetEntry)rel_path.c_str()) == 0) {
        file_data.setFilePath(rel_path);
        file_data.ClientWriteFile(client, ent_path);
      }
    }
  }

  file_list.getBlock().resize(1);
  file_list.getBlock()[0] = '\0';
  file_list.ClientWrite(client);

  UA_Client_disconnect(client);
  UA_Client_delete(client);
  return (int) UA_STATUSCODE_GOOD;
}

