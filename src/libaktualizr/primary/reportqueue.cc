#include "reportqueue.h"

#include <chrono>

#include "config/config.h"

namespace report {

Queue::Queue(const Config& config_in, std::shared_ptr<HttpInterface> http_client)
    : config(config_in), http(std::move(http_client)), shutdown_(false) {
  thread_ = std::thread(std::bind(&Queue::run, this));
}

Queue::~Queue() {
  shutdown_ = true;
  {
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    cv_.notify_all();
  }
  thread_.join();
}

void Queue::run() {
  // Check if queue is nonempty. If so, move any reports to the Json array and
  // try to send it to the server. Clear the Json array only if the send
  // succeeds.
  Json::Value report_array(Json::arrayValue);
  while (!shutdown_) {
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    if (!report_queue_.empty()) {
      std::lock_guard<std::mutex> queue_lock(queue_mutex_);
      while (!report_queue_.empty()) {
        report_array.append(report_queue_.front()->toJson());
        report_queue_.pop();
      }
    }
    if (report_array.size() > 0) {
      HttpResponse response = http->post(config.tls.server + "/events", report_array);
      // 404 implies the server does not support this feature. Nothing we can
      // do, just move along.
      if (response.isOk() || response.http_status_code == 404) {
        report_array = Json::arrayValue;
      }
    }
    cv_.wait_for(thread_lock, std::chrono::seconds(10));
  }
}

void Queue::enqueue(std::unique_ptr<report::Event> event) {
  {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    report_queue_.push(std::move(event));
  }
  cv_.notify_all();
}

Json::Value Event::toJson() {
  Json::Value out;

  out["id"] = id;
  out["deviceTime"] = timestamp.ToString();
  out["eventType"]["id"] = type;
  out["eventType"]["version"] = version;
  out["event"] = custom;

  return out;
}

DownloadComplete::DownloadComplete(const std::string& director_target) : Event("DownloadComplete", 1, Json::Value()) {
  custom = director_target;
}

CampaignAccepted::CampaignAccepted(const std::string& campaign_id) : Event("campaign_accepted", 0, Json::Value()) {
  custom["campaignId"] = campaign_id;
}

};  // namespace report
