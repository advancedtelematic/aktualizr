#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <memory>
#include <string>

#include <boost/signals2.hpp>

#include "libaktualizr/results.h"

/**
 * Aktualizr status events.
 */
namespace event {

/**
 * Base class for all event objects.
 */
class BaseEvent {
 public:
  BaseEvent() = default;
  BaseEvent(std::string variant_in) : variant(std::move(variant_in)) {}
  virtual ~BaseEvent() = default;

  template <typename T>
  bool isTypeOf() {
    return variant == T::TypeName;
  }

  std::string variant;
};

/**
 * Device data has been sent to the server.
 */
class SendDeviceDataComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"SendDeviceDataComplete"};

  SendDeviceDataComplete() { variant = TypeName; }
};

/**
 * A manifest has been sent to the server.
 */
class PutManifestComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"PutManifestComplete"};
  explicit PutManifestComplete(bool success_in) : success(success_in) { variant = TypeName; }
  bool success;
};

/**
 * An update is available for download from the server.
 */
class UpdateCheckComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"UpdateCheckComplete"};
  explicit UpdateCheckComplete(result::UpdateCheck result_in) : result(std::move(result_in)) { variant = TypeName; }

  result::UpdateCheck result;
};

/**
 * A report for a download in progress.
 */
class DownloadProgressReport : public BaseEvent {
 public:
  static constexpr const char* TypeName{"DownloadProgressReport"};

  static bool isDownloadCompleted(const DownloadProgressReport& report) {
    return (report.progress == DownloadProgressReport::ProgressCompletedValue);
  }

 public:
  DownloadProgressReport(Uptane::Target target_in, std::string description_in, unsigned int progress_in)
      : target{std::move(target_in)}, description{std::move(description_in)}, progress{progress_in} {
    variant = TypeName;
  }

  Uptane::Target target;
  std::string description;
  unsigned int progress;

 private:
  static const unsigned int ProgressCompletedValue{100};
};

/**
 * A target has been downloaded.
 */
class DownloadTargetComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"DownloadTargetComplete"};

  DownloadTargetComplete(Uptane::Target update_in, bool success_in)
      : update(std::move(update_in)), success(success_in) {
    variant = TypeName;
  }

  Uptane::Target update;
  bool success;
};

/**
 * All targets for an update have been downloaded.
 */
class AllDownloadsComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"AllDownloadsComplete"};

  explicit AllDownloadsComplete(result::Download result_in) : result(std::move(result_in)) { variant = TypeName; }

  result::Download result;
};

/**
 * An ECU has begun installation of an update.
 */
class InstallStarted : public BaseEvent {
 public:
  static constexpr const char* TypeName{"InstallStarted"};

  explicit InstallStarted(Uptane::EcuSerial serial_in) : serial(std::move(serial_in)) { variant = TypeName; }
  Uptane::EcuSerial serial;
};

/**
 * An installation attempt on an ECU has completed.
 */
class InstallTargetComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"InstallTargetComplete"};

  InstallTargetComplete(Uptane::EcuSerial serial_in, bool success_in)
      : serial(std::move(serial_in)), success(success_in) {
    variant = TypeName;
  }

  Uptane::EcuSerial serial;
  bool success;
};

/**
 * All ECU installation attempts for an update have completed.
 */
class AllInstallsComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"AllInstallsComplete"};

  explicit AllInstallsComplete(result::Install result_in) : result(std::move(result_in)) { variant = TypeName; }

  result::Install result;
};

/**
 * The server has been queried for available campaigns.
 */
class CampaignCheckComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"CampaignCheckComplete"};

  explicit CampaignCheckComplete(result::CampaignCheck result_in) : result(std::move(result_in)) { variant = TypeName; }

  result::CampaignCheck result;
};

/**
 * A campaign has been accepted.
 */
class CampaignAcceptComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"CampaignAcceptComplete"};

  CampaignAcceptComplete() { variant = TypeName; }
};

class CampaignDeclineComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"CampaignDeclineComplete"};

  CampaignDeclineComplete() { variant = TypeName; }
};

class CampaignPostponeComplete : public BaseEvent {
 public:
  static constexpr const char* TypeName{"CampaignPostponeComplete"};

  CampaignPostponeComplete() { variant = TypeName; }
};

using Channel = boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>;

}  // namespace event

#endif  // EVENTS_H_
