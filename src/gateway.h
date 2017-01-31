#ifndef GATEWAY_H_
#define GATEWAY_H_

#include "events.h"
#include <boost/shared_ptr.hpp>

class Gateway {
    public:
        virtual void processEvent(const boost::shared_ptr<event::BaseEvent> &event) = 0;
        virtual ~Gateway(){};
};

#endif /* GATEWAY_H_ */
