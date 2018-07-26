#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <memory>

#include <boost/signals2.hpp>

#include "commands.h"
#include "config/config.h"
#include "events.h"
#include "utilities/channel.h"

/**
 * This class provides the main APIs necessary for launching and controlling
 * libaktualizr.
 */
class Aktualizr {
 public:
  /** Aktualizr requires a configuration object. Examples can be found in the
   *  config directory. */
  explicit Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  /**
   * Launch aktualizr. Depending on the \ref RunningMode in the configuration,
   * this may run indefinitely, so you may want to run this on its own thread.
   */
  int run();
  /**
   * Send a command to the SotaUptaneClient command interpreter.
   * @param command the command to send.
   */
  void sendCommand(const std::shared_ptr<command::BaseCommand>& command);
  /**
   * Provide a function to receive event notifications.
   * @param handler a function that can receive event objects.
   * @return a signal connection object, which can be disconnected if desired.
   */
  boost::signals2::connection setSignalHandler(std::function<void(std::shared_ptr<event::BaseEvent>)>& handler);

 private:
  Config& config_;
  std::shared_ptr<command::Channel> commands_channel_;
  std::shared_ptr<event::Channel> events_channel_;
  std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig_;
};

#endif  // AKTUALIZR_H_
