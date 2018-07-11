/**
\file

Externally facing API for Aktualizr.

Aktualizr is designed to be run as a standalone component on an embedded system and manage the entire software update
process. This is suitable for simple cases, but falls short in more complex environments. For example:

- User consent for updates
- Interactive feedback of installation progress


Some of these are not always necessary. Many users today are satisified with applications (Google Docs/GMail, Office
365, Chrome) that automatically update without user interaction. For these products the designer takes on the
responsibility of making updates seamless, both in terms of zero downtime for the user and that there are no significant
regressions. That is not always the case, and Aktualizr is designed to also work in constrained environments where
updates may make a vehicle undrivable for an extended period of time.

Functional Requirements
=======================

1. Integration with 3rd party HMI
2. Integration with 3rd party interfaces to install software on secondary ECUs
3. Limit network trafic and installation to specific vehicle states
4. Progress bars

Non-functional Requirements
===========================

1. Since these interfaces are often proprietry and under NDA, the implementation must be kept separate from the
Aktualizr tree.
2. It should be possible to have the integration performed by a separate team
3. Flexibility and simple integration into new environments.

Overview
========

The Aktualizr performs all its operations in a background thread, and all the operations except aktualizr_init() and
aktualizr_free() are asynchronous. Notifications occur via callbacks. These calls are made synchronously from
Aktualizr's background thread, so they should complete quickly. Normally the callback will only post a notification the
application's main event loop.


Rejected Alternatives
=====================

It would be possible to allow other components to interface with Aktualizr via
a IPC interface such as D-Bus or Sockets. This would have the some advantages,
such as:

 - Process Isolation
 - Easier to assemble pieces together and hack together things with scripts.
 - Easier privsep

However we believe the disadvantages outweigh them:

- Have to build an IPC mechanism
- Have to build bridges to connect this 'common' IPC mechanism to whatever the end-user system has picked
- New features may require simultanous changes to the aktualizr IPC these bridges.
- Hard to replace built-in functionality with external functionality (e.g. Package Managers)
- Harder to completely replace the high-level workflow.

It would still be possible to write a wrapper that would expose libaktualizr
via an IPC mechanism, but this can be written with a specific use-case and IPC
technology in mind.

\nosubgrouping
*/

struct aktualizr_struct;

/**
 * Aktualizr Handle
 */
typedef aktualizr_struct* aktualizr_t;

struct aktualizr_config_struct;

/** Aktualizr configuration object Handle */
typedef aktualizr_config_struct* aktualizr_config_t;

/**
 * Aktualizr API result code.
 */
typedef enum {
  /** The operation completed successfully */
  AKTUALIZR_OK = 0,
  /** The operation cannot be started because an existing operation is in progress */
  AKTUALIZR_ALREADY_IN_PROGRESS,
  /** The operation cannot be performed with the current Aktualizr configuration */
  AKTUALIZR_BAD_CONFIG,
  /** The device is not provisioned, and provisioning isn't possible and/or wasn't requested */
  AKTUALIZR_NOT_PROVISIONED,
  /** Network communication failed. For example a timeout, DNS failure, no network is present. */
  AKTUALIZR_NETWORK_ERROR,

} aktualizr_res_t;

/**
 * High-level update state suitable for display to a user.
 * This interface is expected to be used by a UI, therefore transient errors
 * like network failure are not exposed here. The system--not the user--will be
 * responsible for retrying in those cases.
 */
typedef enum {
  /** The system is up-to-date with respect to the most recent metadata downloaded. */
  AKTUALIZR_UPDATE_UP_TO_DATE,
  /**
   * A metadata fetch is in progress.
   * A UI may display this identically to \ref AKTUALIZR_UPDATE_UP_TO_DATE, but
   * the state transitions can be used to update a 'last checked for updates 1
   * hour ago' UI component.
   */
  AKTUALIZR_UPDATE_CHECK_IN_PROGRESS,

  /**
   * An update is available.
   */
  AKTUALIZR_UPDATE_AVAILABLE,

  /**
   * An update is downloading
   */
  AKTUALIZR_UPDATE_DOWNLOADING,

  /**
   * An update is installing
   */
  AKTUALIZR_UPDATE_INSTALLING,

  /**
   * An update is already installed, but a reboot is required to start using it.
   */
  AKTUALIZR_UPDATE_AWAITING_REBOOT
} aktualizr_update_state_t;

/**
 * Parameter to \ref aktualizr_run_update to control how much of the update
 * cycle to perform.
 */
typedef enum {
  /** Perform a full update cycle */
  AKTUALIZR_UPDATE_FULL_CYCLE = 1,
  /** Fetch Update metadata only */
  AKTUALIZR_UPDATE_METADATA,
  /** Download only */
  AKTUALIZR_UPDATE_DOWNLOAD,
  /** Install only */
  AKTUALIZR_UPDATE_INSTALL

} aktualizr_poll_operation_t;

/** Callback type for \ref aktualizr_set_state_change_callback */
typedef void (*aktualizr_callback_t)(void* clientp, aktualizr_update_state_t);

/// \name Main API
/// \{

/**
 * Create a new Aktualizr software updater.
 * If polling is enabled in the configuration, this will start polling the
 * update server in the background.
 */
aktualizr_t aktualizr_init(aktualizr_config_t configuration);

/**
 * Shutdown Aktualizr. No other calls to this API may be made after this call.
 */
void aktualizr_free(aktualizr_t aktualizr);

/**
 * Register to be notified when updates are available, downloading or installing.
 * It is not possible to register to multiple callbacks. Passing NULL as a
 * callback will disable state change notifications.
 * \param aktualizr Aktualizr handle
 * \param callback The callback to call, or 'NULL' to disable notifications
 * \param userdata Opaque user handle that is passed to the callback
 */
void aktualizr_set_state_change_callback(aktualizr_t aktualizr, aktualizr_callback_t callback, void* userdata);

/**
 * The current state of the update process.
 */
aktualizr_update_state_t aktualizr_get_update_state(aktualizr_t aktualizr);

/**
 * Perform an update cycle.
 * If Aktualizr isn't currently idle (for example if a polling operation is already in progress) then
 * an \ref AKTUALIZR_ALREADY_IN_PROGRESS error is returned.
 */
aktualizr_res_t aktualizr_run_update(aktualizr_t aktualizr, aktualizr_poll_operation_t operation);

/**
 * Report system information to the server.
 * This includes network, hardware and installed package information.
 * \todo Async? Queued?
 */
aktualizr_res_t aktualizr_report_system_information(aktualizr_t aktualizr);

///
/// \}
///

///
/// \name Aktualizr Configuration
/// \{

/**
 * Create a new configuration object, which is used to build Aktualizr's
 * configuration.
 */
aktualizr_config_t aktualizr_config_init();

/**
 * Free an Aktualizr configuration object.
 * This is needed regardless of whether it is passed to \ref aktualizr_init.
 */
void aktualizr_config_free(aktualizr_config_t config);

/**
 * Load configuration from a file or directory of configuration fragments
 */
aktualizr_res_t aktualizr_config_load(aktualizr_config_t config, char* path);

/**
 * Set log level.
 * \todo Log to places other than stdout (e.g. systemd journal, callbacks)
 */
aktualizr_res_t aktualizr_config_log_level(aktualizr_config_t, int loglevel);

/**
 * Set a single string configuration option.
 */
aktualizr_res_t aktualizr_config_set_string(aktualizr_config_t config, char* section, char* option, char* value);

/**
 * Set a single integer configuration option.
 */
aktualizr_res_t aktualizr_config_set_int(aktualizr_config_t config, char* section, char* option, int value);

/// \}
