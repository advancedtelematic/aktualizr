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

void ReportQueue::enqueue(std::unique_ptr<ReportEvent> event) {
  {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    report_queue_.push(std::move(event));
  }
  cv_.notify_all();
}

void ReportQueue::flushQueue() {
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
}

void ReportEvent::setEcu(const Uptane::EcuSerial& ecu) { custom["ecu"] = ecu.ToString(); }
void ReportEvent::setCorrelationId(const std::string& correlation_id) {
  if (correlation_id != "") {
    custom["correlationId"] = correlation_id;
  }
}

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

EcuDownloadStartedReport::EcuDownloadStartedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id)
    : ReportEvent("ecu_download_started", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
}

EcuDownloadCompletedReport::EcuDownloadCompletedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id,
                                                       bool success)
    : ReportEvent("ecu_download_completed", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
  custom["success"] = success;
}

EcuInstallationStartedReport::EcuInstallationStartedReport(const Uptane::EcuSerial& ecu,
                                                           const std::string& correlation_id)
    : ReportEvent("ecu_installation_started", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
}

EcuInstallationCompletedReport::EcuInstallationCompletedReport(const Uptane::EcuSerial& ecu,
                                                               const std::string& correlation_id, bool success)
    : ReportEvent("ecu_installation_completed", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
  custom["success"] = success;
}
