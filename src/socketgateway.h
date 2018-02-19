#ifndef SOCKETGATEWAY_H_
#define SOCKETGATEWAY_H_

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "gateway.h"

class SocketGateway : public Gateway, boost::noncopyable {
 public:
  SocketGateway(const Config &config_in, command::Channel *commands_channel_in);
  ~SocketGateway() override;
  void processEvent(const boost::shared_ptr<event::BaseEvent> &event) override;

 private:
  const Config &config;
  command::Channel *commands_channel;
  std::vector<int> event_connections;
  std::vector<int> command_connections;
  std::vector<boost::shared_ptr<boost::thread> > command_workers;
  boost::shared_ptr<boost::thread> events_server_thread;
  boost::shared_ptr<boost::thread> commands_server_thread;
  int events_socket;
  int commands_socket;

  void eventsServer();
  void commandsServer();
  void commandsWorker(int socket, command::Channel *channel);
  void broadcast_event(const boost::shared_ptr<event::BaseEvent> &event);
};

#endif /* SOCKETGATEWAY_H_ */
