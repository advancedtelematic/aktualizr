#include "socketgateway.h"

#include <errno.h>
#include <picojson.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "logger.h"

SocketGateway::SocketGateway(const Config &config_in,
                             command::Channel *commands_channel_in)
    : config(config_in), commands_channel(commands_channel_in) {
  unlink(config.network.socket_events_path.c_str());
  unlink(config.network.socket_commands_path.c_str());
  events_server_thread =
      new boost::thread(boost::bind(&SocketGateway::eventsServer, this));
  commands_server_thread =
      new boost::thread(boost::bind(&SocketGateway::commandsServer, this));
}

void SocketGateway::commandsWorker(int socket, command::Channel *channel) {
  int buff_size = 512;
  char buf[buff_size];
  std::string data;

  while (ssize_t bytes = recv(socket, buf, buff_size, MSG_NOSIGNAL)) {
    if (bytes <= 0) break;
    if (bytes < buff_size) {
      buf[bytes] = '\0';
    }
    data.append(buf);
    picojson::value item;
    picojson::default_parse_context ctx(&item);
    picojson::input<std::string::iterator> in(data.begin(), data.end());
    if (picojson::_parse(ctx, in)) {
      data.erase(data.begin(), in.cur());
      *channel << command::BaseCommand::fromPicoJson(item);
    }
  }
  LOGGER_LOG(LVL_error,
             "unix domain socket recv command error, errno: " << errno);
  close(socket);
}

void SocketGateway::eventsServer() {
  int events_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un cli_addr, addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config.network.socket_events_path.c_str(),
          sizeof(addr.sun_path) - 1);
  bind(events_socket, (struct sockaddr *)&addr, sizeof(addr));
  listen(events_socket, 10);
  socklen_t clilen = sizeof(cli_addr);
  while (true) {
    int newsockfd =
        accept(events_socket, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd != -1) {
      event_connections.push_back(newsockfd);
    } else {
      break;
    }
  }
  LOGGER_LOG(LVL_error, "unix domain socket accept error, errno: " << errno);
  close(events_socket);
}

void SocketGateway::commandsServer() {
  int commands_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un cli_addr, addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config.network.socket_commands_path.c_str(),
          sizeof(addr.sun_path) - 1);
  bind(commands_socket, (struct sockaddr *)&addr, sizeof(addr));
  listen(commands_socket, 10);
  socklen_t clilen = sizeof(cli_addr);
  while (true) {
    int newsockfd =
        accept(commands_socket, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd != -1) {
      boost::thread(boost::bind(&SocketGateway::commandsWorker, this, newsockfd,
                                commands_channel));
    } else {
      break;
    }
  }
  LOGGER_LOG(LVL_error, "unix domain socket accept error, errno: " << errno);
  close(commands_socket);
}

SocketGateway::~SocketGateway() {
  for (std::vector<int>::iterator it = event_connections.begin();
       it != event_connections.end(); ++it) {
    close(*it);
  }
  unlink(config.network.socket_events_path.c_str());
  unlink(config.network.socket_commands_path.c_str());
}

void SocketGateway::broadcast_event(
    const boost::shared_ptr<event::BaseEvent> &event) {
  std::string json_event = event->toJson();
  for (std::vector<int>::iterator it = event_connections.begin();
       it != event_connections.end();) {
    ssize_t bytes_sent =
        send(*it, json_event.c_str(), json_event.size(), MSG_NOSIGNAL);
    if (bytes_sent != -1) {
      ++it;
    } else {
      LOGGER_LOG(LVL_error, "unix domain socket write error, errno: " << errno);
      close(*it);
      it = event_connections.erase(it);
    }
  }
}

void SocketGateway::processEvent(
    const boost::shared_ptr<event::BaseEvent> &event) {
  std::vector<std::string>::iterator find_iter =
      std::find(config.network.socket_events.begin(),
                config.network.socket_events.end(), event->variant);
  if (find_iter != config.network.socket_events.end()) {
    broadcast_event(event);
  }
}