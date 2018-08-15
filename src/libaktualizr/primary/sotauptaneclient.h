#include <gtest/gtest.h>
#include <json/json.h>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bootloader/bootloader.h"
#include "commands.h"
#include "config/config.h"
#include "http/httpclient.h"
#include "package_manager/packagemanagerinterface.h"
#include "reportqueue.h"
#include "storage/invstorage.h"
#include "uptane/directorrepository.h"
#include "uptane/fetcher.h"
#include "uptane/imagesrepository.h"
#include "uptane/ipsecondarydiscovery.h"
#include "uptane/secondaryinterface.h"
#include "uptane/uptanerepository.h"
#include "utilities/events.h"

class SotaUptaneClient {
 public:
  static std::shared_ptr<SotaUptaneClient> newDefaultClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                                                            std::shared_ptr<event::Channel> events_channel_in);
  static std::shared_ptr<SotaUptaneClient> newTestClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                                                         std::shared_ptr<HttpInterface> http_client_in,
                                                         std::shared_ptr<event::Channel> events_channel_in = nullptr);
  SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                   std::shared_ptr<HttpInterface> http_client, std::shared_ptr<Uptane::Fetcher> uptane_fetcher_in,
                   std::shared_ptr<Bootloader> bootloader_in, std::shared_ptr<ReportQueue> report_queue_in,
                   std::shared_ptr<event::Channel> events_channel_in);

  bool initialize();
  void addNewSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);
  bool updateMeta();
  bool uptaneIteration();
  bool uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count);
  bool downloadImages(const std::vector<Uptane::Target> &targets);
  void runForever(const std::shared_ptr<command::Channel> &commands_channel);
  Json::Value AssembleManifest();
  std::string secondaryTreehubCredentials() const;
  Uptane::Exception getLastException() const { return last_exception; }

  // ecu_serial => secondary*
  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface> > secondaries;

 private:
  FRIEND_TEST(Uptane, offlineIteration);
  FRIEND_TEST(Uptane, krejectallTest);
  FRIEND_TEST(Uptane, Vector);
  FRIEND_TEST(UptaneCI, OneCycleUpdate);
  bool isInstalledOnPrimary(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, const Uptane::EcuSerial &ecu_id);
  data::InstallOutcome PackageInstall(const Uptane::Target &target);
  void PackageInstallSetResult(const Uptane::Target &target);
  void reportHwInfo();
  void reportInstalledPackages();
  void reportNetworkInfo();
  void init();
  void addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec);
  void verifySecondaries();
  void sendMetadataToEcus(const std::vector<Uptane::Target> &targets);
  void sendImagesToEcus(std::vector<Uptane::Target> targets);
  bool hasPendingUpdates(const Json::Value &manifests);
  void sendDownloadReport();
  bool putManifest();
  bool getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count = nullptr);
  bool downloadTargets(const std::vector<Uptane::Target> &targets);
  void rotateSecondaryRoot(Uptane::RepositoryType repo, Uptane::SecondaryInterface &secondary);
  bool updateDirectorMeta();
  bool updateImagesMeta();
  bool checkImagesMetaOffline();
  bool checkDirectorMetaOffline();

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
  std::shared_ptr<event::Channel> events_channel;
  std::atomic<bool> shutdown = {false};
  Json::Value last_network_info_reported;
  std::map<Uptane::EcuSerial, Uptane::HardwareIdentifier> hw_ids;
  std::map<Uptane::EcuSerial, std::string> installed_images;
  bool installing{false};

  Uptane::Exception last_exception{"", ""};
};

class SerialCompare {
 public:
  explicit SerialCompare(Uptane::EcuSerial target_in) : target(std::move(target_in)) {}
  bool operator()(std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier> &in) { return (in.first == target); }

 private:
  Uptane::EcuSerial target;
};
