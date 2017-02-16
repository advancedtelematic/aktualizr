#ifndef SOCKETGATEWAY_H_
#define SOCKETGATEWAY_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>


#include "gateway.h"
#include "events.h"
#include "config.h"
#include "commands.h"

class SocketGateway: public Gateway {
    public:
        SocketGateway(const Config& config_in, command::Channel* commands_channel_in);
        virtual ~SocketGateway();
        virtual void processEvent(const boost::shared_ptr<event::BaseEvent> &event);
    private:
        Config config;
        command::Channel* commands_channel;
        std::vector<int> event_connections;
        std::vector<int> command_connections;
        std::vector<boost::thread*> command_workers;
        boost::thread *events_server_thread;
        boost::thread *commands_server_thread;
        int events_socket;
        int commands_socket;

        void eventsServer();
        void commandsServer();
        void commandsWorker(int socket, command::Channel *commands_channel);
        void broadcast_event(const boost::shared_ptr<event::BaseEvent> &event);


};

#endif /* SOCKETGATEWAY_H_ */
