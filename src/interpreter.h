#ifndef INTERPRETER_H_
#define INTERPRETER_H_
#include "boost/thread.hpp"
#include "commands.h"
#include "config.h"
#include "logger.h"
#include "sotahttpclient.h"

class Interpreter {
 public:
  Interpreter(const Config &config_in, command::Channel *command_channel_in);
  ~Interpreter();
  void interpret();
  void run();

 private:
  Config config;
  SotaHttpClient sota;
  boost::thread *thread;
  command::Channel *command_channel;
};

#endif