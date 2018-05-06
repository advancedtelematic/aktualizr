#include "opcuabridge_test_utils.h"

#include "logging/logging.h"

namespace opcuabridge_test_utils {

opcuabridge::Signature CreateSignature(const std::string& keyid, const opcuabridge::SignatureMethod& method,
                                       const std::string& hash, const std::string& value) {
  opcuabridge::Signature s;
  s.setKeyid(keyid);
  s.setMethod(method);
  opcuabridge::Hash h;
  h.setFunction(opcuabridge::HASH_FUN_SHA256);
  h.setDigest(hash);
  s.setHash(h);
  s.setValue(value);
  return s;
}

opcuabridge::Signed CreateSigned(std::size_t n) {
  opcuabridge::Signed s;
  std::vector<int> tokens;
  for (int i = 0; i < n; ++i) tokens.push_back(i);
  s.setTokens(tokens);
  s.setTimestamp(time(NULL));
  return s;
}

const std::size_t log_msg_buff_size = 256;

void BoostLogServer(UA_LogLevel level, UA_LogCategory category, const char* msg, va_list args) {
  char msg_buff[log_msg_buff_size];
  vsnprintf(msg_buff, log_msg_buff_size, msg, args);
  BOOST_LOG_STREAM_WITH_PARAMS(
      boost::log::trivial::logger::get(),
      (boost::log::keywords::severity = static_cast<boost::log::trivial::severity_level>(level)))
      << "server " << msg_buff;
}

void BoostLogSecondary(UA_LogLevel level, UA_LogCategory category, const char* msg, va_list args) {
  char msg_buff[log_msg_buff_size];
  vsnprintf(msg_buff, log_msg_buff_size, msg, args);
  BOOST_LOG_STREAM_WITH_PARAMS(
      boost::log::trivial::logger::get(),
      (boost::log::keywords::severity = static_cast<boost::log::trivial::severity_level>(level)))
      << "secondary " << msg_buff;
}

}  // namespace opcuabridge_test_utils
