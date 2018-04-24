#include "socketgateway.h"

#include <errno.h>
#include <picojson.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <utility>

#include "logging/logging.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

using std::make_shared;
using std::shared_ptr;

SocketGateway::SocketGateway(const Config &config_in, std::shared_ptr<command::Channel> commands_channel_in)
    : config(config_in), commands_channel(std::move(std::move(commands_channel_in))) {
  unlink(config.network.socket_events_path.c_str());
  unlink(config.network.socket_commands_path.c_str());
  events_server_thread = make_shared<std::thread>(std::bind(&SocketGateway::eventsServer, this));
  commands_server_thread = make_shared<std::thread>(std::bind(&SocketGateway::commandsServer, this));
}

SocketGateway::~SocketGateway() {
  shutdown(commands_socket, SHUT_RD);
  shutdown(events_socket, SHUT_RD);

  for (auto it = event_connections.begin(); it != event_connections.end(); ++it) {
    close(*it);
  }
  for (auto it = command_connections.begin(); it != command_connections.end(); ++it) {
    close(*it);
  }

  for (auto it = command_workers.begin(); it != command_workers.end(); ++it) {
    (*it)->join();
  }
  commands_server_thread->join();
  events_server_thread->join();

  unlink(config.network.socket_events_path.c_str());
  unlink(config.network.socket_commands_path.c_str());
}

void SocketGateway::commandsWorker(int socket, const std::shared_ptr<command::Channel> &channel) {
  const int buff_size = 512;
  char buf[buff_size];
  std::string data;

  while (ssize_t bytes = recv(socket, buf, buff_size, 0)) {
    if (bytes <= 0) {
      break;
    }
    if (bytes < buff_size) {
      buf[bytes] = '\0';
    }
    data.append(buf);
    picojson::value item;
    picojson::default_parse_context ctx(&item);
    picojson::input<std::string::iterator> in(data.begin(), data.end());
    if (picojson::_parse(ctx, in)) {
      data.erase(data.begin(), in.cur());
      try {
        *channel << command::BaseCommand::fromPicoJson(item);
      } catch (...) {
        LOG_ERROR << "failed command deserealization: " << item.serialize();
      }
    }
  }
  LOG_ERROR << "unix domain socket recv command error, errno: " << errno;
  close(socket);
}

void SocketGateway::eventsServer() {
  events_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un cli_addr {};
  struct sockaddr_un addr {};
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config.network.socket_events_path.c_str(), sizeof(addr.sun_path) - 1);
  bind(events_socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  listen(events_socket, 10);
  socklen_t clilen = sizeof(cli_addr);
  while (true) {
    int newsockfd = accept(events_socket, reinterpret_cast<struct sockaddr *>(&cli_addr), &clilen);
    if (newsockfd != -1) {
      event_connections.push_back(newsockfd);
    } else {
      break;
    }
  }
  LOG_ERROR << "unix domain event socket accept error, errno: " << errno;
  close(events_socket);
}

void SocketGateway::commandsServer() {
  commands_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un cli_addr {};
  struct sockaddr_un addr {};
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, config.network.socket_commands_path.c_str(), sizeof(addr.sun_path) - 1);
  bind(commands_socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  listen(commands_socket, 10);
  socklen_t clilen = sizeof(cli_addr);
  while (true) {
    int newsockfd = accept(commands_socket, reinterpret_cast<struct sockaddr *>(&cli_addr), &clilen);
    if (newsockfd != -1) {
      command_connections.push_back(newsockfd);
      command_workers.push_back(
          make_shared<std::thread>(std::bind(&SocketGateway::commandsWorker, this, newsockfd, commands_channel)));
    } else {
      break;
    }
  }
  LOG_ERROR << "unix domain command socket accept error, errno: " << errno;
  close(commands_socket);
}

void SocketGateway::broadcast_event(const std::shared_ptr<event::BaseEvent> &event) {
  std::string json_event = event->toJson();
  for (auto it = event_connections.begin(); it != event_connections.end();) {
    ssize_t bytes_sent = send(*it, json_event.c_str(), json_event.size(), MSG_NOSIGNAL);
    if (bytes_sent != -1) {
      ++it;
    } else {
      LOG_ERROR << "unix domain socket write error, errno: " << errno;
      close(*it);
      it = event_connections.erase(it);
    }
  }
}

void SocketGateway::processEvent(const std::shared_ptr<event::BaseEvent> &event) {
  auto find_iter = std::find(config.network.socket_events.begin(), config.network.socket_events.end(), event->variant);
  if (find_iter != config.network.socket_events.end()) {
    broadcast_event(event);
  }
}
