#include "api-test-utils.h"

#include <boost/process.hpp>
#include "../tests/httpfake.h"
#include "../tests/uptane_test_common.h"
#include "config/config.h"

void Run_fake_http_server(const char *path) {
  std::string port = TestUtils::getFreePort();
  std::string server = "http://127.0.0.1:" + port;
  Config config;
  config.uptane.repo_server = server;
  boost::process::child http_server_process(path, port, "-f");
  TestUtils::waitForServer(server + "/");
}

UptaneGenerator *Get_uptane_generator(const char *path) { return new Process(path); }

void Run_uptane_generator(UptaneGenerator *generator, const char **args, size_t args_count) {
  std::vector<std::string> args_vector;
  args_vector.reserve(args_count);
  for (size_t i = 0; i < args_count; ++i) {
    args_vector.emplace_back(args[i]);
  }
  generator->run(args_vector);
}

void Remove_uptane_generator(UptaneGenerator *generator) { delete generator; }

TemporaryDirectory *Get_temporary_directory() { return new TemporaryDirectory; }

char *Get_temporary_directory_path(const TemporaryDirectory *dir) {
  std::string path_string = dir->PathString();
  void *path_ptr = malloc(sizeof(char) * (path_string.size() + 1));
  auto *path = static_cast<char *>(path_ptr);
  strncpy(path, path_string.c_str(), path_string.size() + 1);
  return path;
}

void Remove_temporary_directory(TemporaryDirectory *dir) { delete dir; }

Config *Get_test_config(TemporaryDirectory *temp_dir, const char *flavor, TemporaryDirectory *fake_meta_dir) {
  auto http = std::make_shared<HttpFake>(temp_dir->Path(), flavor, fake_meta_dir->Path());
  return new Config(UptaneTestCommon::makeTestConfig(*temp_dir, http->tls_server));
}

void Remove_test_config(Config *config) { delete config; }
