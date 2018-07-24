#ifndef EVENTSINTERPRETER_H_
#define EVENTSINTERPRETER_H_

#include <thread>

#include <boost/signals2.hpp>

#include "commands.h"
#include "config/config.h"
#include "events.h"
#include "logging/logging.h"

class EventsInterpreter {
 public:
  EventsInterpreter(const Config &config_in, std::shared_ptr<event::Channel> events_channel_in,
                    std::shared_ptr<command::Channel> commands_channel_in,
                    std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig_in = nullptr);
  ~EventsInterpreter();
  void interpret();
  void run();

 private:
  std::shared_ptr<command::BaseCommand> handle_cycle(event::BaseEvent &event, bool forever);
  std::shared_ptr<command::BaseCommand> handle_check(event::BaseEvent &event);
  std::shared_ptr<command::BaseCommand> handle_download(event::BaseEvent &event);
  std::shared_ptr<command::BaseCommand> handle_install(event::BaseEvent &event);

  const Config &config;
  std::thread thread;
  std::shared_ptr<event::Channel> events_channel;
  std::shared_ptr<command::Channel> commands_channel;
  std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig;
};

#endif /* EVENTSINTERPRETER_H_ */
