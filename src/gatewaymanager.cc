#include "gateway.h"

#include <boost/make_shared.hpp>

#include "gatewaymanager.h"

#ifdef WITH_GENIVI
#include "dbusgateway/dbusgateway.h"
#endif

GatewayManager::GatewayManager(const Config &config, command::Channel *commands_channel_in) {
#ifdef WITH_GENIVI
  if (config.gateway.dbus) {
    gateways.push_back(boost::shared_ptr<Gateway>(new DbusGateway(config, commands_channel_in)));
  }
#endif
}

void GatewayManager::processEvents(const boost::shared_ptr<event::BaseEvent> &event) {
  for (std::vector<boost::shared_ptr<Gateway> >::iterator it = gateways.begin(); it != gateways.end(); ++it) {
    (*it)->processEvent(event);
  }
}
