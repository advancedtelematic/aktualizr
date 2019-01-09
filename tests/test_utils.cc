#include "test_utils.h"

#include <signal.h>

#if __linux__
#include <sys/prctl.h>
#endif
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include <boost/filesystem.hpp>

std::string TestUtils::getFreePort() {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    std::cout << "socket() failed: " << errno;
    throw std::runtime_error("Could not open socket");
  }
  struct sockaddr_in soc_addr;
  memset(&soc_addr, 0, sizeof(struct sockaddr_in));
  soc_addr.sin_family = AF_INET;
  soc_addr.sin_addr.s_addr = INADDR_ANY;
  soc_addr.sin_port = htons(INADDR_ANY);

  if (bind(s, (struct sockaddr *)&soc_addr, sizeof(soc_addr)) == -1) {
    std::cout << "bind() failed: " << errno;
    throw std::runtime_error("Could not bind socket");
  }

  struct sockaddr_in sa;
  unsigned int sa_len = sizeof(sa);
  if (getsockname(s, (struct sockaddr *)&sa, &sa_len) == -1) {
    throw std::runtime_error("getsockname failed");
  }
  close(s);
  return std::to_string(ntohs(sa.sin_port));
}

void TestUtils::writePathToConfig(const boost::filesystem::path &toml_in, const boost::filesystem::path &toml_out,
                                  const boost::filesystem::path &storage_path) {
  // Append our temp_dir path as storage.path to the config file. This is a hack
  // but less annoying than the alternatives.
  boost::filesystem::copy_file(toml_in, toml_out);
  std::string conf_path_str = toml_out.string();
  std::ofstream cs(conf_path_str.c_str(), std::ofstream::app);
  cs << "\n[storage]\npath = " << storage_path.string() << "\n";
}

void TestUtils::waitForServer(const std::string &address) {
  CURL *handle = curl_easy_init();
  if (handle == nullptr) {
    throw std::runtime_error("Could not initialize curl handle");
  }
  curlEasySetoptWrapper(handle, CURLOPT_URL, address.c_str());
  curlEasySetoptWrapper(handle, CURLOPT_CONNECTTIMEOUT, 3L);
  curlEasySetoptWrapper(handle, CURLOPT_NOBODY, 1L);
  curlEasySetoptWrapper(handle, CURLOPT_VERBOSE, 1L);
  curlEasySetoptWrapper(handle, CURLOPT_SSL_VERIFYPEER, 0L);

  CURLcode result;
  for (size_t counter = 1; counter <= 100; counter++) {
    result = curl_easy_perform(handle);
    if (result == 0) {
      // connection successful
      break;
    }
    if (counter % 5 == 0) {
      std::cout << "Unable to connect to " << address << " after " << counter << " tries.\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  curl_easy_cleanup(handle);

  if (result != 0) {
    throw std::runtime_error("Wait for server timed out");
  }
}

void TestHelperProcess::run(const char *argv0, const char *args[]) {
  pid_ = fork();
  if (pid_ == -1) {
    throw std::runtime_error("Failed to execute process:" + std::string(argv0));
  }
  if (pid_ == 0) {
#if __linux__
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
    execvp(argv0, const_cast<char *const *>(args));
    std::cout << "Could not execute child " << argv0 << "\n";
    exit(1);
  }
}

TestHelperProcess::~TestHelperProcess() {
  assert(pid_);
  kill(pid_, SIGINT);
}
