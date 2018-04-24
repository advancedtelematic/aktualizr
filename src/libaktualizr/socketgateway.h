#ifndef SOCKETGATEWAY_H_
#define SOCKETGATEWAY_H_

#include <thread>
#include <vector>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "gateway.h"

class SocketGateway : public Gateway {
 public:
  SocketGateway(const Config &config_in, std::shared_ptr<command::Channel> commands_channel_in);
  SocketGateway(const SocketGateway &) = delete;
  SocketGateway operator=(const SocketGateway &) = delete;

  ~SocketGateway() override;
  void processEvent(const std::shared_ptr<event::BaseEvent> &event) override;

 private:
  const Config &config;
  std::shared_ptr<command::Channel> commands_channel;
  std::vector<int> event_connections;
  std::vector<int> command_connections;
  std::vector<std::shared_ptr<std::thread> > command_workers;
  std::shared_ptr<std::thread> events_server_thread;
  std::shared_ptr<std::thread> commands_server_thread;
  int events_socket{};
  int commands_socket{};

  void eventsServer();
  void commandsServer();
  void commandsWorker(int socket, const std::shared_ptr<command::Channel> &channel);
  void broadcast_event(const std::shared_ptr<event::BaseEvent> &event);
};

#endif /* SOCKETGATEWAY_H_ */
