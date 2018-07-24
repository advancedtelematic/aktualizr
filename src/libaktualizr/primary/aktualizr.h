#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <memory>

#include <boost/signals2.hpp>

#include "commands.h"
#include "config/config.h"
#include "events.h"
#include "utilities/channel.h"

class Aktualizr {
 public:
  explicit Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  int run();
  void sendCommand(std::shared_ptr<command::BaseCommand> command);
  void setSignalHandler(std::function<void(std::shared_ptr<event::BaseEvent>)>& handler);

 private:
  Config& config_;
  std::shared_ptr<command::Channel> commands_channel_;
  std::shared_ptr<event::Channel> events_channel_;
  std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig_;
};

#endif  // AKTUALIZR_H_
