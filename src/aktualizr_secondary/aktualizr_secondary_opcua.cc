#include "aktualizr_secondary_opcua.h"

#include <boost/make_unique.hpp>

AktualizrSecondaryOpcua::AktualizrSecondaryOpcua(const AktualizrSecondaryConfig& config,
                                                 std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondaryCommon(config, storage), running_(true), delegate_(this) {
  server_ = boost::make_unique<opcuabridge::Server>(&delegate_, config_.network.port);
}

void AktualizrSecondaryOpcua::run() { server_->run(&running_); }

void AktualizrSecondaryOpcua::stop() { running_ = false; }
