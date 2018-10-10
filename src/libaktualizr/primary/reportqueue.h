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

class ReportEvent {
 public:
  std::string id;
  std::string type;
  int version;
  Json::Value custom;
  TimeStamp timestamp;

  Json::Value toJson();

 protected:
  ReportEvent(std::string event_type, int event_version, const Json::Value& event_custom)
      : id(Utils::randomUuid()),
        type(std::move(event_type)),
        version(event_version),
        custom(event_custom),
        timestamp(TimeStamp::Now()) {}
};

class DownloadCompleteReport : public ReportEvent {
 public:
  DownloadCompleteReport(const std::string& director_target);
};

class CampaignAcceptedReport : public ReportEvent {
 public:
  CampaignAcceptedReport(const std::string& campaign_id);
};

class ReportQueue {
 public:
  ReportQueue(const Config& config_in, std::shared_ptr<HttpInterface> http_client);
  ~ReportQueue();
  void run();
  void enqueue(std::unique_ptr<ReportEvent> event);

 private:
  const Config& config;
  std::shared_ptr<HttpInterface> http;
  std::thread thread_;
  std::condition_variable cv_;
  std::mutex thread_mutex_;
  std::mutex queue_mutex_;
  std::queue<std::unique_ptr<ReportEvent>> report_queue_;
  bool shutdown_;
};

#endif  // REPORTQUEUE_H_
