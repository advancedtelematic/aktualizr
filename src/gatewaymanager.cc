#include "gateway.h"

#include "gatewaymanager.h"
#include "socketgateway.h"

GatewayManager::GatewayManager(const Config &config, std::shared_ptr<command::Channel> commands_channel_in) {
  if (config.gateway.socket) {
    gateways.push_back(std::make_shared<SocketGateway>(config, commands_channel_in));
  }
}

void GatewayManager::processEvents(const std::shared_ptr<event::BaseEvent> &event) {
  for (auto it = gateways.begin(); it != gateways.end(); ++it) {
    (*it)->processEvent(event);
  }
}
