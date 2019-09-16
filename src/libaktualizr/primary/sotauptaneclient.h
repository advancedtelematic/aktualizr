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

#include "bootloader/bootloader.h"
#include "campaign/campaign.h"
#include "config/config.h"
#include "http/httpclient.h"
#include "package_manager/packagemanagerinterface.h"
#include "primary/events.h"
#include "primary/results.h"
#include "reportqueue.h"
#include "storage/invstorage.h"
#include "uptane/directorrepository.h"
#include "uptane/exceptions.h"
#include "uptane/fetcher.h"
#include "uptane/imagesrepository.h"
#include "uptane/iterator.h"
#include "uptane/secondaryinterface.h"

class SotaUptaneClient {
 public:
  static std::shared_ptr<SotaUptaneClient> newDefaultClient(
      Config &config_in, std::shared_ptr<INvStorage> storage_in,
      std::shared_ptr<event::Channel> events_channel_in = nullptr);

  SotaUptaneClient(Config &config_in, const std::shared_ptr<INvStorage> &storage_in,
                   std::shared_ptr<HttpInterface> http_client, std::shared_ptr<Bootloader> bootloader_in,
                   std::shared_ptr<ReportQueue> report_queue_in,
                   std::shared_ptr<event::Channel> events_channel_in = nullptr);
  ~SotaUptaneClient();

  void initialize();
  void addNewSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);
  result::Download downloadImages(const std::vector<Uptane::Target> &targets,
                                  const api::FlowControlToken *token = nullptr);
  std::pair<bool, Uptane::Target> downloadImage(const Uptane::Target &target,
                                                const api::FlowControlToken *token = nullptr);
  void reportPause();
  void reportResume();
  void sendDeviceData();
  result::UpdateCheck fetchMeta();
  bool putManifest(const Json::Value &custom = Json::nullValue);
  result::UpdateCheck checkUpdates();
  result::Install uptaneInstall(const std::vector<Uptane::Target> &updates);
  result::CampaignCheck campaignCheck();
  void campaignAccept(const std::string &campaign_id);
  void campaignDecline(const std::string &campaign_id);
  void campaignPostpone(const std::string &campaign_id);
  bool hasPendingUpdates();
  bool isInstallCompletionRequired();
  void completeInstall();
  Uptane::LazyTargetsList allTargets();
  Uptane::Target getCurrent() { return package_manager_->getCurrent(); }

  bool updateImagesMeta();  // TODO: make private once aktualizr has a proper TUF API
  bool checkImagesMetaOffline();
  data::InstallationResult PackageInstall(const Uptane::Target &target);
  TargetStatus VerifyTarget(const Uptane::Target &target) { return package_manager_->verifyTarget(target); }

 protected:
  void addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, DeviceInstallationResult);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckNoUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Aktualizr, FinalizationFailure);
  FRIEND_TEST(Aktualizr, InstallationFailure);
  FRIEND_TEST(Aktualizr, AutoRebootAfterUpdate);
  FRIEND_TEST(Aktualizr, EmptyTargets);
  FRIEND_TEST(Aktualizr, FullOstreeUpdate);
  FRIEND_TEST(Aktualizr, DownloadNonOstreeBin);
  FRIEND_TEST(DockerAppManager, DockerApp_Fetch);
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
  FRIEND_TEST(UptaneVector, Test);
  FRIEND_TEST(aktualizr_secondary_uptane, credentialsPassing);
  friend class CheckForUpdate;       // for load tests
  friend class ProvisionDeviceTask;  // for load tests

  bool uptaneIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  bool uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  result::UpdateStatus checkUpdatesOffline(const std::vector<Uptane::Target> &targets);
  Json::Value AssembleManifest();
  std::string secondaryTreehubCredentials() const;
  Uptane::Exception getLastException() const { return last_exception; }
  bool isInstalledOnPrimary(const Uptane::Target &target);
  static std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets,
                                                const Uptane::EcuSerial &ecu_id);
  data::InstallationResult PackageInstallSetResult(const Uptane::Target &target);
  void finalizeAfterReboot();
  void reportHwInfo();
  void reportInstalledPackages();
  void reportNetworkInfo();
  void verifySecondaries();
  void sendMetadataToEcus(const std::vector<Uptane::Target> &targets);
  std::future<bool> sendFirmwareAsync(Uptane::SecondaryInterface &secondary, const std::shared_ptr<std::string> &data);
  std::vector<result::Install::EcuReport> sendImagesToEcus(const std::vector<Uptane::Target> &targets);

  bool putManifestSimple(const Json::Value &custom = Json::nullValue);
  bool getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count = nullptr);
  void rotateSecondaryRoot(Uptane::RepositoryType repo, Uptane::SecondaryInterface &secondary);
  bool updateDirectorMeta();
  bool checkDirectorMetaOffline();
  void computeDeviceInstallationResult(data::InstallationResult *result, const std::string &correlation_id);
  std::unique_ptr<Uptane::Target> findTargetInDelegationTree(const Uptane::Target &target, bool offline);
  std::unique_ptr<Uptane::Target> findTargetHelper(const Uptane::Targets &cur_targets,
                                                   const Uptane::Target &queried_target, int level, bool terminating,
                                                   bool offline);

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
  Uptane::ImagesRepository images_repo;
  Uptane::Manifest uptane_manifest;
  std::shared_ptr<INvStorage> storage;
  std::shared_ptr<PackageManagerInterface> package_manager_;
  std::shared_ptr<HttpInterface> http;
  std::shared_ptr<Uptane::Fetcher> uptane_fetcher;
  std::shared_ptr<Bootloader> bootloader;
  std::shared_ptr<ReportQueue> report_queue;
  Json::Value last_network_info_reported;
  Uptane::EcuMap hw_ids;
  std::shared_ptr<event::Channel> events_channel;
  boost::signals2::connection conn;
  Uptane::Exception last_exception{"", ""};
  // ecu_serial => secondary*
  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>> secondaries;
  std::mutex download_mutex;
};

class TargetCompare {
 public:
  explicit TargetCompare(const Uptane::Target &target_in) : target(target_in) {}
  bool operator()(const Uptane::Target &in) const { return (in.MatchTarget(target)); }

 private:
  const Uptane::Target &target;
};

class SerialCompare {
 public:
  explicit SerialCompare(Uptane::EcuSerial serial_in) : serial(std::move(serial_in)) {}
  bool operator()(const std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier> &in) const {
    return (in.first == serial);
  }

 private:
  const Uptane::EcuSerial serial;
};

#endif  // SOTA_UPTANE_CLIENT_H_
