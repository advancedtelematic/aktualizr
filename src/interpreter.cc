#include "interpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>

#include "config.h"

Interpreter::Interpreter(const Config &config_in,
                         command::Channel *command_channel_in)
    : config(config_in), sota(config), command_channel(command_channel_in) {}

Interpreter::~Interpreter() { delete thread; }

void Interpreter::interpret() {
  thread = new boost::thread(boost::bind(&Interpreter::run, this));
}

void Interpreter::run() {
  command::BaseCommand command;
  while (*command_channel >> command) {
    if (command.variant == "GetUpdateRequests") {
      std::vector<data::UpdateRequest> updates = sota.getAvailableUpdates();
      for (std::vector<data::UpdateRequest>::iterator update = updates.begin();
           update != updates.end(); ++update) {
        LOGGER_LOG(LVL_info, "we have a new  update");
        Json::Value status = sota.downloadUpdate(*update);
        LOGGER_LOG(LVL_info, "update have been downloaded");
        sota.reportUpdateResult(status);
      }
    }
  }
}