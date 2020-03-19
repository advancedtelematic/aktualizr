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
#include "storage/invstorage.h"
#include "uptane/tuf.h"

class ReportEvent {
 public:
  std::string id;
  std::string type;
  int version;
  Json::Value custom;
  TimeStamp timestamp;

  Json::Value toJson() const;

 protected:
  ReportEvent(std::string event_type, int event_version)
      : id(Utils::randomUuid()), type(std::move(event_type)), version(event_version), timestamp(TimeStamp::Now()) {}

  void setEcu(const Uptane::EcuSerial& ecu);
  void setCorrelationId(const std::string& correlation_id);
};

class CampaignAcceptedReport : public ReportEvent {
 public:
  CampaignAcceptedReport(const std::string& campaign_id);
};

class CampaignDeclinedReport : public ReportEvent {
 public:
  CampaignDeclinedReport(const std::string& campaign_id);
};

class CampaignPostponedReport : public ReportEvent {
 public:
  CampaignPostponedReport(const std::string& campaign_id);
};

class DevicePausedReport : public ReportEvent {
 public:
  DevicePausedReport(const std::string& correlation_id);
};

class DeviceResumedReport : public ReportEvent {
 public:
  DeviceResumedReport(const std::string& correlation_id);
};

class EcuDownloadStartedReport : public ReportEvent {
 public:
  EcuDownloadStartedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id);
};

class EcuDownloadCompletedReport : public ReportEvent {
 public:
  EcuDownloadCompletedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id, bool success);
};

class EcuInstallationStartedReport : public ReportEvent {
 public:
  EcuInstallationStartedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id);
};

class EcuInstallationAppliedReport : public ReportEvent {
 public:
  EcuInstallationAppliedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id);
};

class EcuInstallationCompletedReport : public ReportEvent {
 public:
  EcuInstallationCompletedReport(const Uptane::EcuSerial& ecu, const std::string& correlation_id, bool success);
};

class ReportQueue {
 public:
  ReportQueue(const Config& config_in, std::shared_ptr<HttpInterface> http_client,
              std::shared_ptr<INvStorage> storage_in);
  ~ReportQueue();
  void run();
  void enqueue(std::unique_ptr<ReportEvent> event);

 private:
  void flushQueue();

  const Config& config;
  std::shared_ptr<HttpInterface> http;
  std::thread thread_;
  std::condition_variable cv_;
  std::mutex m_;
  std::queue<std::unique_ptr<ReportEvent>> report_queue_;
  bool shutdown_{false};
  std::shared_ptr<INvStorage> storage;
};

#endif  // REPORTQUEUE_H_
