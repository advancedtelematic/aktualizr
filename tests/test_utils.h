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

/**
 * Launch a simple child subprocess, automatically kill it the object is destroyed
 */
class TestHelperProcess {
 public:
  /**
   * Launch the process: executable then arguments
   *
   * note: it uses templates for argument types but `args` are expected to be
   * `std::string`
   */
  template <typename... Strings>
  TestHelperProcess(const std::string &argv0, const Strings &... args) {
    const int size = sizeof...(args);
    const char *cmd_args[size + 2] = {argv0.c_str(), (args.c_str())..., nullptr};

    run(cmd_args[0], cmd_args);
  }
  ~TestHelperProcess();

 private:
  void run(const char *argv0, const char *args[]);

  pid_t pid_;
};

#endif  // TEST_UTILS_H_
