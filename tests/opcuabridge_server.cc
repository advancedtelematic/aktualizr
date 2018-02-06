#include <open62541.h>

#include "logging.h"

#include "opcuabridge_test_utils.h"

#include <signal.h>
#include <cstdlib>
#include <fstream>

void BoostLog(UA_LogLevel level, UA_LogCategory category, const char* msg, va_list args) {
    const std::size_t msg_buff_sz = 256; char msg_buff[msg_buff_sz];
    vsnprintf(msg_buff, msg_buff_sz, msg, args);
    BOOST_LOG_STREAM_WITH_PARAMS(
      boost::log::trivial::logger::get(),
      (boost::log::keywords::severity = static_cast<boost::log::trivial::severity_level>(level)))
      << "server " << msg_buff;
}

UA_Boolean running = true;

static void stopHandler(int sign) {
  LOG_INFO << "received ctrl-c";
  running = false;
}

int main(int argc, char *argv[]) {
  UA_UInt16 port = (argc < 2 ? 4840 : (UA_UInt16)std::atoi(argv[1]));

  logger_init();

  LOG_INFO << "server port number: " << argv[1];

  struct sigaction sa;
  sa.sa_handler = &stopHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  BOOST_PP_LIST_FOR_EACH(DEFINE_MESSAGE, _, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  UA_ServerConfig *config = UA_ServerConfig_new_minimal(port, NULL);
  config->logger = &BoostLog;

  UA_Server *server = UA_Server_new(config);

  BOOST_PP_LIST_FOR_EACH(INIT_SERVER_NODESET, server, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

  UA_StatusCode retval = UA_Server_run(server, &running);
  UA_Server_delete(server);
  UA_ServerConfig_delete(config);

  return retval;
}
