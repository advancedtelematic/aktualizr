#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "utilities/utils.h"

struct TestUtils {
  static std::string getFreePort();
  static void writePathToConfig(const boost::filesystem::path &toml_in, const boost::filesystem::path &toml_out,
                                const boost::filesystem::path &storage_path);
};

class TestHelperProcess {
 public:
  TestHelperProcess(const std::string &argv0, const std::string &argv1, const std::string &argv2 = "");
  ~TestHelperProcess();

 private:
  pid_t pid_;
};

#endif  // TEST_UTILS_H_
