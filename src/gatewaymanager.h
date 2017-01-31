#ifndef GATEWAYMANAGER_H_
#define GATEWAYMANAGER_H_

#include <vector>
#include <boost/shared_ptr.hpp>
#include "gateway.h"
#include "config.h"
#include "events.h"
#include "commands.h"


class GatewayManager {
    public:
        GatewayManager(const Config &config_in, command::Channel *commands_channel_in);
        void processEvents(const boost::shared_ptr<event::BaseEvent> &event);

    private:
        std::vector<boost::shared_ptr<Gateway> > gateways;

        
};

#endif /* GATEWAYMANAGER_H_ */
