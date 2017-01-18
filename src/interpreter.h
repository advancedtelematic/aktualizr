#ifndef INTERPRETER_H_
#define INTERPRETER_H_
#include "boost/thread.hpp"
#include "commands.h"
#include "config.h"
#include "events.h"
#include "logger.h"
#include "sotahttpclient.h"

class Interpreter {
 public:
  Interpreter(const Config &config_in, command::Channel *commands_channel_in,
              event::Channel *events_channel_in);
  ~Interpreter();
  void interpret();
  void run();

 private:
  Config config;
  SotaHttpClient sota;
  boost::thread *thread;
  command::Channel *commands_channel;
  event::Channel *events_channel;
};

#endif
