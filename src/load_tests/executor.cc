#include "executor.h"

std::atomic_bool InterruptableExecutionController::interrupted{false};
