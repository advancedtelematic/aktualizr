#include "test_utils.h"

#include <signal.h>
#include <sys/wait.h>
#include <thread>

#if __linux__
#include <sys/prctl.h>
#endif
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include <boost/asio/io_service.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

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

Process::Result Process::spawn(const std::string &executable_to_run, const std::vector<std::string> &executable_args) {
  std::future<std::string> output;
  std::future<std::string> err_output;
  std::future<int> child_process_exit_code;
  boost::asio::io_service io_service;

  try {
    boost::process::child child_process(boost::process::exe = executable_to_run, boost::process::args = executable_args,
                                        boost::process::std_out > output, boost::process::std_err > err_output,
                                        boost::process::on_exit = child_process_exit_code, io_service);

    io_service.run();

    bool wait_successfull = child_process.wait_for(std::chrono::seconds(60));
    if (!wait_successfull) {
      throw std::runtime_error("Timeout occured while waiting for a child process completion");
    }

    // Issue with getting an exit code if 'on_exit' handler is not specified
    // https://github.com/boostorg/process/issues/39
    // child_process_exit_code = child_process.exit_code();

  } catch (const std::exception &exc) {
    throw std::runtime_error("Failed to spawn process " + executable_to_run + " exited with an error: " + exc.what());
  }

  return std::make_tuple(child_process_exit_code.get(), output.get(), err_output.get());
}

Process::Result Process::run(const std::vector<std::string> &args) {
  last_exit_code_ = 0;
  last_stdout_.clear();
  last_stderr_.clear();

  auto cred_gen_result = Process::spawn(exe_path_, args);
  std::tie(last_exit_code_, last_stdout_, last_stderr_) = cred_gen_result;

  return cred_gen_result;
}
