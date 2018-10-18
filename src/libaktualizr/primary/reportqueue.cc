#include "reportqueue.h"

#include <chrono>

#include "config/config.h"

ReportQueue::ReportQueue(const Config& config_in, std::shared_ptr<HttpInterface> http_client)
    : config(config_in), http(std::move(http_client)), shutdown_(false) {
  thread_ = std::thread(std::bind(&ReportQueue::run, this));
}

ReportQueue::~ReportQueue() {
  shutdown_ = true;
  {
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    cv_.notify_all();
  }
  thread_.join();

  LOG_DEBUG << "Flushing report queue";
  flushQueue();
}

void ReportQueue::run() {
  // Check if queue is nonempty. If so, move any reports to the Json array and
  // try to send it to the server. Clear the Json array only if the send
  // succeeds.
  while (!shutdown_) {
    std::unique_lock<std::mutex> thread_lock(thread_mutex_);
    flushQueue();
    cv_.wait_for(thread_lock, std::chrono::seconds(10));
  }
}

std::future<bool> ReportQueue::enqueue(std::unique_ptr<ReportEvent> event) {
  event->promise = std_::make_unique<std::promise<bool>>();
  auto report_future = event->promise->get_future();
  {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    report_queue_.emplace_back(std::unique_ptr<ReportEvent>(std::move(event)));
  }
  cv_.notify_all();
  return report_future;
}

void ReportQueue::flushQueue() {
  std::lock_guard<std::mutex> queue_lock(queue_mutex_);
  Json::Value report_array(Json::arrayValue);
  for (const auto& item : report_queue_) {
    report_array.append(item->toJson());
  }

  if (report_array.size() > 0) {
    HttpResponse response = http->post(config.tls.server + "/events", report_array);
    // 404 implies the server does not support this feature. Nothing we can
    // do, just move along.
    if (response.isOk() || response.http_status_code == 404) {
      for (const auto& item : report_queue_) {
        item->promise->set_value(true);
      }
      report_queue_.clear();
    } else {
      if (report_queue_.size() > 20) {  // report_queue_ shouldn't grow infinitely
        for (const auto& item : report_queue_) {
          item->promise->set_value(false);
        }
        report_queue_.clear();
      }
    }
  }
}

void ReportEvent::setEcu(const Uptane::EcuSerial& ecu) { custom["ecu"] = ecu.ToString(); }

Json::Value ReportEvent::toJson() {
  Json::Value out;

  out["id"] = id;
  out["deviceTime"] = timestamp.ToString();
  out["eventType"]["id"] = type;
  out["eventType"]["version"] = version;
  out["event"] = custom;

  return out;
}

DownloadCompleteReport::DownloadCompleteReport(const std::string& director_target)
    : ReportEvent("DownloadComplete", 1) {
  custom = director_target;
}

CampaignAcceptedReport::CampaignAcceptedReport(const std::string& campaign_id) : ReportEvent("campaign_accepted", 0) {
  custom["campaignId"] = campaign_id;
}

EcuDownloadStartedReport::EcuDownloadStartedReport(const Uptane::EcuSerial& ecu)
    : ReportEvent("ecu_download_started", 0) {
  setEcu(ecu);
}

EcuDownloadCompletedReport::EcuDownloadCompletedReport(const Uptane::EcuSerial& ecu, bool success)
    : ReportEvent("ecu_download_completed", 0) {
  setEcu(ecu);
  custom["success"] = success;
}

EcuInstallationStartedReport::EcuInstallationStartedReport(const Uptane::EcuSerial& ecu)
    : ReportEvent("ecu_installation_started", 0) {
  setEcu(ecu);
}

EcuInstallationCompletedReport::EcuInstallationCompletedReport(const Uptane::EcuSerial& ecu, bool success)
    : ReportEvent("ecu_installation_completed", 0) {
  setEcu(ecu);
  custom["success"] = success;
}
