#ifndef SOTA_UPTANE_CLIENT_H_
#define SOTA_UPTANE_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/signals2.hpp>
#include "gtest/gtest_prod.h"
#include "json/json.h"

#include "libaktualizr/campaign.h"
#include "libaktualizr/config.h"
#include "libaktualizr/events.h"
#include "libaktualizr/packagemanagerfactory.h"
#include "libaktualizr/packagemanagerinterface.h"
#include "libaktualizr/results.h"
#include "libaktualizr/secondaryinterface.h"

#include "bootloader/bootloader.h"
#include "http/httpclient.h"
#include "primary/secondary_provider_builder.h"
#include "reportqueue.h"
#include "uptane/directorrepository.h"
#include "uptane/exceptions.h"
#include "uptane/fetcher.h"
#include "uptane/imagerepository.h"
#include "uptane/iterator.h"
#include "uptane/manifest.h"
#include "uptane/tuf.h"
#include "utilities/apiqueue.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in,
                   std::shared_ptr<event::Channel> events_channel_in,
                   const Uptane::EcuSerial &primary_serial = Uptane::EcuSerial::Unknown(),
                   const Uptane::HardwareIdentifier &hwid = Uptane::HardwareIdentifier::Unknown())
      : config(config_in),
        storage(std::move(storage_in)),
        http(std::move(http_in)),
        package_manager_(PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, http)),
        uptane_fetcher(new Uptane::Fetcher(config, http)),
        events_channel(std::move(events_channel_in)),
        primary_ecu_serial_(primary_serial),
        primary_ecu_hw_id_(hwid) {
    report_queue = std_::make_unique<ReportQueue>(config, http, storage);
    secondary_provider_ = SecondaryProviderBuilder::Build(config, storage, package_manager_);
  }

  SotaUptaneClient(Config &config_in, const std::shared_ptr<INvStorage> &storage_in,
                   std::shared_ptr<HttpInterface> http_in)
      : SotaUptaneClient(config_in, storage_in, std::move(http_in), nullptr) {}

  SotaUptaneClient(Config &config_in, const std::shared_ptr<INvStorage> &storage_in)
      : SotaUptaneClient(config_in, storage_in, std::make_shared<HttpClient>()) {}

  void initialize();
  void addSecondary(const std::shared_ptr<SecondaryInterface> &sec);
  result::Download downloadImages(const std::vector<Uptane::Target> &targets,
                                  const api::FlowControlToken *token = nullptr);
  std::pair<bool, Uptane::Target> downloadImage(const Uptane::Target &target,
                                                const api::FlowControlToken *token = nullptr);
  void reportPause();
  void reportResume();
  void sendDeviceData(const Json::Value &custom_hwinfo = Json::nullValue);
  result::UpdateCheck fetchMeta();
  bool putManifest(const Json::Value &custom = Json::nullValue);
  result::UpdateCheck checkUpdates();
  result::Install uptaneInstall(const std::vector<Uptane::Target> &updates);
  result::CampaignCheck campaignCheck();
  void campaignAccept(const std::string &campaign_id);
  void campaignDecline(const std::string &campaign_id);
  void campaignPostpone(const std::string &campaign_id);

  bool hasPendingUpdates() const;
  bool isInstallCompletionRequired() const;
  void completeInstall() const;
  Uptane::LazyTargetsList allTargets() const;
  Uptane::Target getCurrent() const { return package_manager_->getCurrent(); }
  std::vector<Uptane::Target> getStoredTargets() const { return package_manager_->getTargetFiles(); }
  void deleteStoredTarget(const Uptane::Target &target) { package_manager_->removeTargetFile(target); }
  std::ifstream openStoredTarget(const Uptane::Target &target) {
    auto status = package_manager_->verifyTarget(target);
    if (status == TargetStatus::kGood) {
      return package_manager_->openTargetFile(target);
    } else {
      throw std::runtime_error("Failed to open Target");
    }
  }

  void updateImageMeta();  // TODO: make private once aktualizr has a proper TUF API
  void checkImageMetaOffline();
  data::InstallationResult PackageInstall(const Uptane::Target &target);
  TargetStatus VerifyTarget(const Uptane::Target &target) const { return package_manager_->verifyTarget(target); }

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, DeviceInstallationResult);
  FRIEND_TEST(Aktualizr, DeviceInstallationResultMetadata);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckNoUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Aktualizr, FinalizationFailure);
  FRIEND_TEST(Aktualizr, InstallationFailure);
  FRIEND_TEST(Aktualizr, AutoRebootAfterUpdate);
  FRIEND_TEST(Aktualizr, EmptyTargets);
  FRIEND_TEST(Aktualizr, FullOstreeUpdate);
  FRIEND_TEST(Aktualizr, DownloadNonOstreeBin);
  FRIEND_TEST(Uptane, AssembleManifestGood);
  FRIEND_TEST(Uptane, AssembleManifestBad);
  FRIEND_TEST(Uptane, InstallFakeGood);
  FRIEND_TEST(Uptane, restoreVerify);
  FRIEND_TEST(Uptane, PutManifest);
  FRIEND_TEST(Uptane, offlineIteration);
  FRIEND_TEST(Uptane, IgnoreUnknownUpdate);
  FRIEND_TEST(Uptane, kRejectAllTest);
  FRIEND_TEST(UptaneCI, ProvisionAndPutManifest);
  FRIEND_TEST(UptaneCI, CheckKeys);
  FRIEND_TEST(UptaneKey, Check);  // Note hacky name
  FRIEND_TEST(UptaneNetwork, DownloadFailure);
  FRIEND_TEST(UptaneNetwork, LogConnectivityRestored);
  FRIEND_TEST(UptaneVector, Test);
  FRIEND_TEST(aktualizr_secondary_uptane, credentialsPassing);
  friend class CheckForUpdate;       // for load tests
  friend class ProvisionDeviceTask;  // for load tests

  void uptaneIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  void uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  result::UpdateStatus checkUpdatesOffline(const std::vector<Uptane::Target> &targets);
  Json::Value AssembleManifest();
  std::exception_ptr getLastException() const { return last_exception; }
  static std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets,
                                                const Uptane::EcuSerial &ecu_id);
  data::InstallationResult PackageInstallSetResult(const Uptane::Target &target);
  void finalizeAfterReboot();
  void reportHwInfo(const Json::Value &custom_hwinfo);
  void reportInstalledPackages();
  void reportNetworkInfo();
  void reportAktualizrConfiguration();
  bool waitSecondariesReachable(const std::vector<Uptane::Target> &updates);
  void storeInstallationFailure(const data::InstallationResult &result);
  data::InstallationResult rotateSecondaryRoot(Uptane::RepositoryType repo, SecondaryInterface &secondary);
  void sendMetadataToEcus(const std::vector<Uptane::Target> &targets, data::InstallationResult *result,
                          std::string *raw_installation_report);
  std::future<data::InstallationResult> sendFirmwareAsync(SecondaryInterface &secondary, const Uptane::Target &target);
  std::vector<result::Install::EcuReport> sendImagesToEcus(const std::vector<Uptane::Target> &targets);

  bool putManifestSimple(const Json::Value &custom = Json::nullValue);
  void getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count = nullptr);
  void updateDirectorMeta();
  void checkDirectorMetaOffline();
  void computeDeviceInstallationResult(data::InstallationResult *result, std::string *raw_installation_report) const;
  std::unique_ptr<Uptane::Target> findTargetInDelegationTree(const Uptane::Target &target, bool offline);
  std::unique_ptr<Uptane::Target> findTargetHelper(const Uptane::Targets &cur_targets,
                                                   const Uptane::Target &queried_target, int level, bool terminating,
                                                   bool offline);
  void checkAndUpdatePendingSecondaries();
  const Uptane::EcuSerial &primaryEcuSerial() const { return primary_ecu_serial_; }
  boost::optional<Uptane::HardwareIdentifier> getEcuHwId(const Uptane::EcuSerial &serial) const;

  template <class T, class... Args>
  void sendEvent(Args &&... args) {
    std::shared_ptr<event::BaseEvent> event = std::make_shared<T>(std::forward<Args>(args)...);
    if (events_channel) {
      (*events_channel)(std::move(event));
    } else if (!event->isTypeOf<event::DownloadProgressReport>()) {
      LOG_INFO << "got " << event->variant << " event";
    }
  }

  Config &config;
  Uptane::DirectorRepository director_repo;
  Uptane::ImageRepository image_repo;
  Uptane::ManifestIssuer::Ptr uptane_manifest;
  std::shared_ptr<INvStorage> storage;
  std::shared_ptr<HttpInterface> http;
  std::shared_ptr<PackageManagerInterface> package_manager_;
  std::shared_ptr<Uptane::Fetcher> uptane_fetcher;
  std::unique_ptr<ReportQueue> report_queue;
  std::shared_ptr<SecondaryProvider> secondary_provider_;
  std::shared_ptr<event::Channel> events_channel;
  boost::signals2::scoped_connection conn;
  std::exception_ptr last_exception;
  // ecu_serial => secondary*
  std::map<Uptane::EcuSerial, SecondaryInterface::Ptr> secondaries;
  std::mutex download_mutex;
  Uptane::EcuSerial primary_ecu_serial_;
  Uptane::HardwareIdentifier primary_ecu_hw_id_;
};

class TargetCompare {
 public:
  explicit TargetCompare(const Uptane::Target &target_in) : target(target_in) {}
  bool operator()(const Uptane::Target &in) const { return (in.MatchTarget(target)); }

 private:
  const Uptane::Target &target;
};

#endif  // SOTA_UPTANE_CLIENT_H_
