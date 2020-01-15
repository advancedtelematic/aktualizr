#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "utilities/utils.h"

struct TestUtils {
  static std::string getFreePort();
  static in_port_t getFreePortAsInt();
  static void writePathToConfig(const boost::filesystem::path &toml_in, const boost::filesystem::path &toml_out,
                                const boost::filesystem::path &storage_path);
  static void waitForServer(const std::string &address);
};

class Process {
 public:
  using Result = std::tuple<int, std::string, std::string>;

  static Result spawn(const std::string &executable_to_run, const std::vector<std::string> &executable_args);

  Process(const std::string &exe_path) : exe_path_(exe_path) {}

  Process::Result run(const std::vector<std::string> &args);

  int lastExitCode() const { return last_exit_code_; }

  const std::string &lastStdOut() const { return last_stdout_; }

  const std::string &lastStdErr() const { return last_stderr_; }

 private:
  const std::string exe_path_;

  int last_exit_code_;
  std::string last_stdout_;
  std::string last_stderr_;
};

#endif  // TEST_UTILS_H_
