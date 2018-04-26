#ifndef EVENTSINTERPRETER_H_
#define EVENTSINTERPRETER_H_

#include <thread>
#include "commands.h"
#include "config/config.h"
#include "events.h"
#include "gatewaymanager.h"
#include "logging/logging.h"

class EventsInterpreter {
 public:
  EventsInterpreter(const Config &config_in, std::shared_ptr<event::Channel> events_channel_in,
                    std::shared_ptr<command::Channel> commands_channel_in);
  ~EventsInterpreter();
  void interpret();
  void run();

 private:
  const Config &config;
  std::thread thread;
  std::shared_ptr<event::Channel> events_channel;
  std::shared_ptr<command::Channel> commands_channel;
  GatewayManager gateway_manager;
};

#endif /* EVENTSINTERPRETER_H_ */
