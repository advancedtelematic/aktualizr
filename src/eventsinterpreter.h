#ifndef EVENTSINTERPRETER_H_
#define EVENTSINTERPRETER_H_

#include "boost/thread.hpp"
#include "commands.h"
#include "config.h"
#include "events.h"
#include "gatewaymanager.h"
#include "logging.h"

class EventsInterpreter {
 public:
  EventsInterpreter(const Config &config_in, event::Channel *events_channel_in, command::Channel *commands_channel_in);
  ~EventsInterpreter();
  void interpret();
  void run();

 private:
  const Config &config;
  boost::thread thread;
  event::Channel *events_channel;
  command::Channel *commands_channel;
  GatewayManager gateway_manager;
};

#endif /* EVENTSINTERPRETER_H_ */
