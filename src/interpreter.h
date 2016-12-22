#ifndef INTERPRETER_H_
#define INTERPRETER_H_
#include "config.h"
#include "logger.h"
#include "sotahttpclient.h"

class Interpreter {
 public:
  Interpreter(const Config &config_in) : config(config_in), sota(config) {}
  void run();

 private:
  Config config;
  SotaHttpClient sota;
};

#endif