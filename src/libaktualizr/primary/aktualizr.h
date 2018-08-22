#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <memory>

#include <boost/signals2.hpp>

#include "commands.h"
#include "config/config.h"
#include "eventsinterpreter.h"
#include "sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"
#include "utilities/channel.h"
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

  /**
   * Launch aktualizr. Depending on the \ref RunningMode in the configuration,
   * this may run indefinitely, so you may want to run this on its own thread.
   */
  int Run();

  /**
   * Asynchronously shutdown Aktualizr
   */
  void Shutdown();

  /**
   * Asynchronously perform a check for campaigns.
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   */
  void CampaignCheck();

  /**
   * Asynchronously accept a campaign for the current device
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   */
  void CampaignAccept(const std::string& campaign_id);

  /**
   * Asynchronously send local device data to the server.
   * This includes network status, installed packages, hardware etc.
   */
  void SendDeviceData();

  /**
   * Asynchronously fetch Uptane metadata.
   * This collects a client manifest, PUTs it to the director, then updates
   * the Uptane metadata, including root and targets.
   */
  void FetchMetadata();

  /**
   * Asynchronously load already-fetched Uptane metadata from disk.
   * This is only needed when the metadata fetch and downloads/installation are
   * in separate aktualizr runs.
   */
  void CheckUpdates();

  /**
   * Asynchronously download targets.
   */
  void Download(std::vector<Uptane::Target> updates);

  /**
   * Asynchronously install targets.
   */

  void Install(std::vector<Uptane::Target> updates);

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
  Config& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<command::Channel> commands_channel_;
  std::shared_ptr<event::Channel> events_channel_;
  std::shared_ptr<SotaUptaneClient> uptane_client_;
  std::shared_ptr<EventsInterpreter> events_interpreter_;
  std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig_;
};

#endif  // AKTUALIZR_H_
