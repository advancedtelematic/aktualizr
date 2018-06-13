#include "reportqueue.h"

#include <chrono>

#include "config/config.h"

ReportQueue::ReportQueue(const Config& config_in, HttpInterface& http_client)
    : config(config_in), http(http_client), shutdown_(false) {
  thread_ = std::thread(std::bind(&ReportQueue::run, this));
}

ReportQueue::~ReportQueue() {
  shutdown_ = true;
  {
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    cv_.notify_all();
  }
  thread_.join();
}

void ReportQueue::run() {
  while (!shutdown_) {
    bool more = false;
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    if (!report_queue_.empty()) {
      Json::Value report_array(Json::arrayValue);
      std::lock_guard<std::mutex> queue_lock(queue_mutex_);
      report_array.append(*report_queue_.front());
      HttpResponse response = http.post(config.tls.server + "/events", report_array);
      // 404 implies the server does not support this feature. Nothing we can
      // do, just move along.
      if (response.isOk() || response.http_status_code == 404) {
        report_queue_.pop();
        if (!report_queue_.empty()) {
          more = true;
        }
      }
    }
    // Skip sleeping if there is more to send and the last send was successful.
    if (!more) {
      cv_.wait_for(thread_lock, std::chrono::seconds(10));
    }
  }
}

void ReportQueue::enqueue(std::unique_ptr<Json::Value> report) {
  {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    report_queue_.push(std::move(report));
  }
  cv_.notify_all();
}
