#ifndef EVENTSINTERPRETER_H_
#define EVENTSINTERPRETER_H_

#include "boost/thread.hpp"
#include "events.h"
#include "commands.h"
#include "config.h"
#include "logger.h"
#include "gatewaymanager.h"

class EventsInterpreter {
 public:
  EventsInterpreter(const Config &config_in, event::Channel *events_channel_in, command::Channel * commands_channel_in);
  ~EventsInterpreter();
  void interpret();
  void run();

 private:
  Config config;
  boost::thread *thread;
  event::Channel *events_channel;
  command::Channel *commands_channel;
  GatewayManager gateway_manager;
};



#endif /* EVENTSINTERPRETER_H_ */
