#ifndef REPORTQUEUE_H_
#define REPORTQUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <json/json.h>

#include "config/config.h"
#include "http/httpclient.h"
#include "logging/logging.h"

namespace report {

class Event {
 public:
  std::string id;
  std::string type;
  int version;
  Json::Value custom;
  TimeStamp timestamp;

  Json::Value toJson();

 protected:
  Event(std::string event_type, int event_version, const Json::Value& event_custom)
      : id(Utils::randomUuid()),
        type(std::move(event_type)),
        version(event_version),
        custom(event_custom),
        timestamp(TimeStamp::Now()) {}
};

class DownloadComplete : public Event {
 public:
  DownloadComplete(const std::string& director_target);
};

class CampaignAccepted : public Event {
 public:
  CampaignAccepted(const std::string& campaign_id);
};

class Queue {
 public:
  Queue(const Config& config_in, std::shared_ptr<HttpInterface> http_client);
  ~Queue();
  void run();
  void enqueue(std::unique_ptr<report::Event> event);

 private:
  const Config& config;
  std::shared_ptr<HttpInterface> http;
  std::thread thread_;
  std::condition_variable cv_;
  std::mutex thread_mutex_;
  std::mutex queue_mutex_;
  std::queue<std::unique_ptr<report::Event>> report_queue_;
  bool shutdown_;
};

};  // namespace report

#endif  // REPORTQUEUE_H_
