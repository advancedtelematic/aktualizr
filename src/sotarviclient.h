#ifndef SOTARVICLIENT_H
#define SOTARVICLIENT_H
#include "rvi.h"

#include <string>
#include <map>
#include <json/json.h>

#include "config.h"
#include "events.h"
#include "types.h"
#include "commands.h"

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using namespace boost::archive::iterators;

typedef transform_width< binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6 > base64_to_bin;


class SotaRVIClient
{
public:
    SotaRVIClient(const Config& config, event::Channel * events_channel_in);
    ~SotaRVIClient();
    void run();
    void sendEvent(const boost::shared_ptr<event::BaseEvent> &event);
    void startDownload(const data::UpdateRequestId &update_id);
    void invokeAck(const std::string &update_id);
    void saveChunk(const Json::Value &chunk_json);
    void downloadComplete(const Json::Value &parameters);
    void sendUpdateReport(data::UpdateReport update_report);
    void runForever(command::Channel *commands_channel);


private:
    TRviHandle rvi;
    std::map<std::string, std::vector<int> > chunks_map;
    const Config &config;
    int connection;
    event::Channel * events_channel;
    Json::Value services;
    bool stop;
    boost::thread thread;


};

#endif // SOTARVICLIENT_H
