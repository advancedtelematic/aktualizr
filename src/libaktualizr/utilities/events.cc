#include "events.h"

#include <utility>

namespace event {

UpdateCheckComplete::UpdateCheckComplete(UpdateCheckResult result_in) : result(std::move(result_in)) {
  variant = "UpdateCheckComplete";
}

SendDeviceDataComplete::SendDeviceDataComplete() { variant = "SendDeviceDataComplete"; }

PutManifestComplete::PutManifestComplete(bool success_in) : success(success_in) { variant = "PutManifestComplete"; }

DownloadProgressReport::DownloadProgressReport(Uptane::Target target_in, std::string description_in,
                                               unsigned int progress_in)
    : target{std::move(target_in)}, description{std::move(description_in)}, progress{progress_in} {
  variant = "DownloadProgressReport";
}

DownloadTargetComplete::DownloadTargetComplete(Uptane::Target update_in, bool success_in)
    : update(std::move(update_in)), success(success_in) {
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

AllInstallsComplete::AllInstallsComplete(InstallResult result_in) : result(std::move(result_in)) {
  variant = "AllInstallsComplete";
}

CampaignCheckComplete::CampaignCheckComplete(CampaignCheckResult result_in) : result(std::move(result_in)) {
  variant = "CampaignCheckComplete";
}

CampaignAcceptComplete::CampaignAcceptComplete() { variant = "CampaignAcceptComplete"; }

}  // namespace event
