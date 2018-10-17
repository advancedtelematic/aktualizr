#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <atomic>
#include <future>
#include <memory>

#include <gtest/gtest.h>
#include <boost/signals2.hpp>

#include "config/config.h"
#include "sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"
#include "utilities/events.h"

/**
 * This class provides the main APIs necessary for launching and controlling
 * libaktualizr.
 */
class Aktualizr {
 public:
  /** Aktualizr requires a configuration object. Examples can be found in the
   *  config directory. */
  explicit Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  /*
   * Initialize aktualizr. Any secondaries should be added before making this
   * call. This will provision with the server if required. This must be called
   * before using any other aktualizr functions except AddSecondary.
   */
  void Initialize();

  /**
   * Run aktualizr indefinitely until a Shutdown event is received. Intended to
   * be used with the Full \ref RunningMode setting. You may want to run this on
   * its own thread.
   */
  int Run();

  /**
   * Asynchronously shut aktualizr down.
   */
  void Shutdown();

  /**
   * Check for campaigns.
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   */
  std::vector<campaign::Campaign> CampaignCheck();

  /**
   * Accept a campaign for the current device
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   */
  void CampaignAccept(const std::string& campaign_id);

  /**
   * Send local device data to the server.
   * This includes network status, installed packages, hardware etc.
   */
  void SendDeviceData();

  /**
   * Fetch Uptane metadata and check for updates.
   * This collects a client manifest, PUTs it to the director, updates the
   * Uptane metadata (including root and targets), and then checks the metadata
   * for target updates.
   */
  std::vector<Uptane::Target> CheckUpdates();

  /**
   * Download targets.
   */
  std::pair<bool, std::vector<Uptane::Target>> Download(const std::vector<Uptane::Target>& updates);

  /**
   * Install targets.
   */
  void Install(const std::vector<Uptane::Target>& updates);

  /**
   * Run an uptane cycle.
   *
   * Behaviour depends on the configured running mode (full cycle, check and
   * download or check and install)
   */
  void UptaneCycle();

  /**
   * Add new secondary to aktualizr.
   */
  void AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface>& secondary);

  /**
   * Provide a function to receive event notifications.
   * @param handler a function that can receive event objects.
   * @return a signal connection object, which can be disconnected if desired.
   */
  boost::signals2::connection SetSignalHandler(std::function<void(std::shared_ptr<event::BaseEvent>)>& handler);

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, FullWithUpdates);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckWithUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Aktualizr, InstallWithUpdates);
  FRIEND_TEST(Aktualizr, CampaignCheck);
  Aktualizr(Config& config, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<SotaUptaneClient> uptane_client_in,
            std::shared_ptr<event::Channel> sig_in);
  void systemSetup();

  Config& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<SotaUptaneClient> uptane_client_;
  std::shared_ptr<event::Channel> sig_;
  std::atomic<bool> shutdown_ = {false};

  struct CycleEventHandler {
    Aktualizr& aktualizr;
    std::mutex m{};
    bool running{true};
    std::promise<bool> finished{};
    std::future<bool> fut{};

    CycleEventHandler(Aktualizr& akt);
    void breakLoop();
    void handle(const std::shared_ptr<event::BaseEvent>& event);
  };
};

#endif  // AKTUALIZR_H_
