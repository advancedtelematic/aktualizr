#include "events.h"

#include <utility>

namespace event {

Error::Error(std::string message_in) : message(std::move(message_in)) { variant = "Error"; }

UpdateCheckComplete::UpdateCheckComplete(UpdateCheckResult result_in) : result(std::move(result_in)) {
  variant = "UpdateCheckComplete";
}

SendDeviceDataComplete::SendDeviceDataComplete() { variant = "SendDeviceDataComplete"; }

PutManifestComplete::PutManifestComplete() { variant = "PutManifestComplete"; }

DownloadProgressReport::DownloadProgressReport(Uptane::Target target_in, std::string description_in,
                                               unsigned int progress_in)
    : target{std::move(target_in)}, description{std::move(description_in)}, progress{progress_in} {
  variant = "DownloadProgressReport";
}

DownloadTargetComplete::DownloadTargetComplete(Uptane::Target update_in) : update(std::move(update_in)) {
  variant = "DownloadTargetComplete";
}

AllDownloadsComplete::AllDownloadsComplete(DownloadResult result_in) : result(std::move(result_in)) {
  variant = "AllDownloadsComplete";
}

InstallStarted::InstallStarted(Uptane::EcuSerial serial_in) : serial(std::move(serial_in)) {
  variant = "InstallStarted";
}

InstallTargetComplete::InstallTargetComplete(Uptane::EcuSerial serial_in, bool success_in)
    : serial(std::move(serial_in)), success(success_in) {
  variant = "InstallTargetComplete";
}

AllInstallsComplete::AllInstallsComplete() { variant = "AllInstallsComplete"; }

CampaignCheckComplete::CampaignCheckComplete(CampaignCheckResult result_in) : result(std::move(result_in)) {
  variant = "CampaignCheckComplete";
}

CampaignAcceptComplete::CampaignAcceptComplete() { variant = "CampaignAcceptComplete"; }

}  // namespace event
