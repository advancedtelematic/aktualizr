#include "aktualizr_secondary_opcua.h"
#include "socket_activation.h"

#include <boost/make_unique.hpp>

AktualizrSecondaryOpcua::AktualizrSecondaryOpcua(const AktualizrSecondaryConfig& config,
                                                 std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondaryCommon(config, storage), running_(true), delegate_(this) {
  if (socket_activation::listen_fds(0) >= 1) {
    LOG_INFO << "Use socket activation";
    server_ = boost::make_unique<opcuabridge::Server>(&delegate_,
                                                      socket_activation::listen_fds_start + 0,
                                                      socket_activation::listen_fds_start + 1,
                                                      config_.network.port);
  } else {
    server_ = boost::make_unique<opcuabridge::Server>(&delegate_, config_.network.port);
  }
}

void AktualizrSecondaryOpcua::run() { server_->run(&running_); }

void AktualizrSecondaryOpcua::stop() { running_ = false; }
