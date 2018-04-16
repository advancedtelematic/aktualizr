#ifndef GATEWAY_H_
#define GATEWAY_H_

#include "events.h"

class Gateway {
 public:
  virtual void processEvent(const std::shared_ptr<event::BaseEvent> &event) = 0;
  virtual ~Gateway() = default;
};

#endif /* GATEWAY_H_ */
