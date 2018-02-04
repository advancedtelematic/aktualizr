#include <open62541.h>

#include "opcuabridge_test_utils.h"

#include <signal.h>
#include <cstdlib>
#include <fstream>

UA_Boolean running = true;
UA_Logger logger = UA_Log_Stdout;

static void stopHandler(int sign) {
  UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "received ctrl-c");
  running = false;
}

int main(int argc, char *argv[]) {
  UA_UInt16 port = (argc < 2 ? 4840 : (UA_UInt16)std::atoi(argv[1]));

  UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "server port number: %s", argv[1]);

  struct sigaction sa;
  sa.sa_handler = &stopHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  BOOST_PP_LIST_FOR_EACH(DEFINE_MESSAGE, _, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  UA_ServerConfig *config = UA_ServerConfig_new_minimal(port, NULL);
  UA_Server *server = UA_Server_new(config);

  BOOST_PP_LIST_FOR_EACH(INIT_SERVER_NODESET, server, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  UA_StatusCode retval = UA_Server_run(server, &running);
  UA_Server_delete(server);
  UA_ServerConfig_delete(config);

  return retval;
}
