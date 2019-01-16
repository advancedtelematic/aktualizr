#ifndef SOTA_UPTANE_CLIENT_H_
#define SOTA_UPTANE_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <boost/signals2.hpp>
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
#include "uptane/fetcher.h"
#include "uptane/imagesrepository.h"
#include "uptane/ipsecondarydiscovery.h"
#include "uptane/secondaryinterface.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  static std::shared_ptr<SotaUptaneClient> newDefaultClient(
      Config &config_in, std::shared_ptr<INvStorage> storage_in,
      std::shared_ptr<event::Channel> events_channel_in = nullptr);
  static std::shared_ptr<SotaUptaneClient> newTestClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                                                         std::shared_ptr<HttpInterface> http_client_in,
                                                         std::shared_ptr<event::Channel> events_channel_in = nullptr);
  SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                   std::shared_ptr<HttpInterface> http_client, std::shared_ptr<Bootloader> bootloader_in,
                   std::shared_ptr<ReportQueue> report_queue_in,
                   std::shared_ptr<event::Channel> events_channel_in = nullptr);
  ~SotaUptaneClient();

  void initialize();
  void addNewSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);
  result::Download downloadImages(const std::vector<Uptane::Target> &targets);
  result::Pause pause();
  result::Pause resume();
  void sendDeviceData();
  result::UpdateCheck fetchMeta();
  bool putManifest();
  result::UpdateCheck checkUpdates();
  result::Install uptaneInstall(const std::vector<Uptane::Target> &updates);
  void installationComplete(const std::shared_ptr<event::BaseEvent> &event);
  result::CampaignCheck campaignCheck();
  void campaignAccept(const std::string &campaign_id);

  const std::vector<Uptane::Target> GetRepoTargets();

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckNoUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Uptane, AssembleManifestGood);
  FRIEND_TEST(Uptane, AssembleManifestBad);
  FRIEND_TEST(Uptane, InstallFake);
  FRIEND_TEST(Uptane, restoreVerify);
  FRIEND_TEST(Uptane, PutManifest);
  FRIEND_TEST(Uptane, offlineIteration);
  FRIEND_TEST(Uptane, IgnoreUnknownUpdate);
  FRIEND_TEST(Uptane, kRejectAllTest);
  FRIEND_TEST(Uptane, Vector);  // Note hacky name (see uptane_vector_tests.cc)
  FRIEND_TEST(UptaneCI, ProvisionAndPutManifest);
  FRIEND_TEST(UptaneCI, CheckKeys);
  FRIEND_TEST(UptaneKey, Check);  // Note hacky name
  FRIEND_TEST(UptaneNetwork, DownloadFailure);
  FRIEND_TEST(aktualizr_secondary_uptane, credentialsPassing);
  friend class CheckForUpdate;       // for load tests
  friend class ProvisionDeviceTask;  // for load tests

  bool updateMeta();
  bool uptaneIteration();
  bool uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  Json::Value AssembleManifest();
  std::string secondaryTreehubCredentials() const;
  Uptane::Exception getLastException() const { return last_exception; }
  bool isInstalledOnPrimary(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, const Uptane::EcuSerial &ecu_id);
  data::InstallOutcome PackageInstall(const Uptane::Target &target);
  data::OperationResult PackageInstallSetResult(const Uptane::Target &target);
  void finalizeAfterReboot();
  void reportHwInfo();
  void reportInstalledPackages();
  void reportNetworkInfo();
  void addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);
  void verifySecondaries();
  void sendMetadataToEcus(const std::vector<Uptane::Target> &targets);
  std::future<bool> sendFirmwareAsync(Uptane::SecondaryInterface &secondary, const std::shared_ptr<std::string> &data);
  std::vector<result::InstallReport> sendImagesToEcus(const std::vector<Uptane::Target> &targets);
  bool hasPendingUpdates();
  void sendDownloadReport();

  bool putManifestSimple();
  bool getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count = nullptr);
  bool downloadTargets(const std::vector<Uptane::Target> &targets);
  std::pair<bool, Uptane::Target> downloadImage(Uptane::Target target);
  void rotateSecondaryRoot(Uptane::RepositoryType repo, Uptane::SecondaryInterface &secondary);
  bool updateDirectorMeta();
  bool updateImagesMeta();
  bool checkImagesMetaOffline();
  bool checkDirectorMetaOffline();

  template <class T, class... Args>
  void sendEvent(Args &&... args) {
    std::shared_ptr<event::BaseEvent> event = std::make_shared<T>(std::forward<Args>(args)...);
    if (events_channel) {
      (*events_channel)(std::move(event));
    } else if (event->variant != "DownloadProgressReport") {
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
  const std::shared_ptr<Bootloader> bootloader;
  std::shared_ptr<ReportQueue> report_queue;
  Json::Value last_network_info_reported;
  std::map<Uptane::EcuSerial, Uptane::HardwareIdentifier> hw_ids;
  std::shared_ptr<event::Channel> events_channel;
  boost::signals2::connection conn;
  Uptane::Exception last_exception{"", ""};
  // ecu_serial => secondary*
  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>> secondaries;
  std::mutex download_mutex;
};

class SerialCompare {
 public:
  explicit SerialCompare(Uptane::EcuSerial target_in) : target(std::move(target_in)) {}
  bool operator()(std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier> &in) { return (in.first == target); }

 private:
  Uptane::EcuSerial target;
};

#endif  // SOTA_UPTANE_CLIENT_H_
