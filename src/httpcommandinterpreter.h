#ifndef HTTPCOMMANDINTERPRETER_H_
#define HTTPCOMMANDINTERPRETER_H_
#include "boost/thread.hpp"
#include "commands.h"
#include "config.h"
#include "events.h"
#include "logger.h"
#include "sotahttpclient.h"

class HttpCommandInterpreter {
 public:
  HttpCommandInterpreter(const Config &config_in, command::Channel *commands_channel_in,
              event::Channel *events_channel_in);
  ~HttpCommandInterpreter();
  void interpret();
  void run();

 private:
  Config config;
  SotaHttpClient sota;
  boost::thread *thread;
  command::Channel *commands_channel;
  event::Channel *events_channel;
};

#endif // HTTPCOMMANDINTERPRETER_H_
