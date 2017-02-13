#ifndef COMMANDINTERPRETER_H_
#define COMMANDINTERPRETER_H_
#include "boost/thread.hpp"
#include "commands.h"
#include "config.h"
#include "events.h"
#include "logger.h"
#include "sotaclient.h"

class CommandInterpreter {
 public:
  CommandInterpreter(SotaClient *sota_in, command::Channel *commands_channel_in,
              event::Channel *events_channel_in);
  ~CommandInterpreter();
  void interpret();
  void run();

 private:
  Config config;
  SotaClient *sota;
  boost::thread *thread;
  command::Channel *commands_channel;
  event::Channel *events_channel;
};

#endif // COMMANDINTERPRETER_H_
