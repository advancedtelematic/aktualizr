#include "interpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

void Interpreter::run() {
  while (true) {
    boost::property_tree::ptree updates = sota.getAvailableUpdates();
    for (boost::property_tree::ptree::iterator update = updates.begin();
         update != updates.end(); ++update) {
      LOGGER_LOG(LVL_info, "we have a new  update");
      boost::property_tree::ptree status = sota.downloadUpdate(update->second);
      LOGGER_LOG(LVL_info, "update have been downloaded");
      sota.reportUpdateResult(status);
    }
    sleep((unsigned int)config.core.polling_sec);
  }
}