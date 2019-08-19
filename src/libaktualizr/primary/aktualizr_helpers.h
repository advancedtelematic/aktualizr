#ifndef AKTUALIZR_HELPERS_H_
#define AKTUALIZR_HELPERS_H_

#include <memory>
#include "aktualizr.h"

// add as a signal handler to remove old targets just after an installation
// completes
void targets_autoclean_cb(Aktualizr &aktualizr, const std::shared_ptr<event::BaseEvent> &event);

#endif  // AKTUALIZR_HELPERS_H_
