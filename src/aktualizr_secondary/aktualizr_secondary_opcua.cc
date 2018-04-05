#include "aktualizr_secondary_opcua.h"

AktualizrSecondaryOpcua::AktualizrSecondaryOpcua(const AktualizrSecondaryConfig& config)
    : running_(true), config_(config) {
  server_ = std::make_unique<opcuabridge::Server>(&delegate_, config_.network.port);
}

void AktualizrSecondaryOpcua::run() { server_->run(&running_); }

void AktualizrSecondaryOpcua::stop() { running_ = false; }
