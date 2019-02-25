#include "reportqueue.h"

#include <chrono>

#include "config/config.h"

ReportQueue::ReportQueue(const Config& config_in, std::shared_ptr<HttpInterface> http_client)
    : config(config_in), http(std::move(http_client)) {
  thread_ = std::thread(std::bind(&ReportQueue::run, this));
}

ReportQueue::~ReportQueue() {
  {
    std::lock_guard<std::mutex> lock(m_);
    shutdown_ = true;
  }
  cv_.notify_all();
  thread_.join();

  LOG_DEBUG << "Flushing report queue";
  flushQueue();
}

void ReportQueue::run() {
  // Check if queue is nonempty. If so, move any reports to the Json array and
  // try to send it to the server. Clear the Json array only if the send
  // succeeds.
  std::unique_lock<std::mutex> lock(m_);
  while (!shutdown_) {
    flushQueue();
    cv_.wait_for(lock, std::chrono::seconds(10));
  }
}

void ReportQueue::enqueue(std::unique_ptr<ReportEvent> event) {
  {
    std::lock_guard<std::mutex> lock(m_);
    report_queue_.push(std::move(event));
  }
  cv_.notify_all();
}

void ReportQueue::flushQueue() {
  while (!report_queue_.empty()) {
    report_array.append(report_queue_.front()->toJson());
    report_queue_.pop();
  }

  if (config.tls.server.empty()) {
    // Prevent a lot of unnecessary garbage output in uptane vector tests.
    LOG_TRACE << "No server specified. Clearing report queue.";
    report_array.clear();
  }

  if (!report_array.empty()) {
    HttpResponse response = http->post(config.tls.server + "/events", report_array);
    // 404 implies the server does not support this feature. Nothing we can
    // do, just move along.
    if (response.isOk() || response.http_status_code == 404) {
      LOG_TRACE << "Server does not support event reports. Clearing report queue.";
      report_array.clear();
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

CampaignAcceptedReport::CampaignAcceptedReport(const std::string& campaign_id) : ReportEvent("campaign_accepted", 0) {
  custom["campaignId"] = campaign_id;
}

DevicePausedReport::DevicePausedReport(const std::string& correlation_id) : ReportEvent("DevicePaused", 0) {
  setCorrelationId(correlation_id);
}

DeviceResumedReport::DeviceResumedReport(const std::string& correlation_id) : ReportEvent("DeviceResumed", 0) {
  setCorrelationId(correlation_id);
}

EcuDownloadStartedReport::EcuDownloadStartedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id)
    : ReportEvent("EcuDownloadStarted", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
}

EcuDownloadCompletedReport::EcuDownloadCompletedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id,
                                                       bool success)
    : ReportEvent("EcuDownloadCompleted", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
  custom["success"] = success;
}

EcuInstallationStartedReport::EcuInstallationStartedReport(const Uptane::EcuSerial& ecu,
                                                           const std::string& correlation_id)
    : ReportEvent("EcuInstallationStarted", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
}

EcuInstallationAppliedReport::EcuInstallationAppliedReport(const Uptane::EcuSerial& ecu,
                                                           const std::string& correlation_id)
    : ReportEvent("EcuInstallationApplied", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
}

EcuInstallationCompletedReport::EcuInstallationCompletedReport(const Uptane::EcuSerial& ecu,
                                                               const std::string& correlation_id, bool success)
    : ReportEvent("EcuInstallationCompleted", 0) {
  setEcu(ecu);
  setCorrelationId(correlation_id);
  custom["success"] = success;
}
