#ifndef AKTUALIZR_API_H_
#define AKTUALIZR_API_H_

/**
\file

Externally facing API for \aktualizr.

\aktualizr is designed to be run as a standalone component on an embedded
system and manage the entire software update process. This is suitable for
simple cases, but falls short in more complex environments. For example:

- User consent for updates
- Interactive feedback of installation progress

Some of these are not always necessary. Many users today are satisfied with
applications (Google Docs/GMail, Office 365, Chrome) that automatically update
without user interaction. For these products the designer takes on the
responsibility of making updates seamless, both in terms of zero downtime for
the user and that there are no significant regressions. That is not always the
case, and \aktualizr is designed to also work in constrained environments where
updates may make a vehicle undrivable for an extended period of time.

Functional Requirements
=======================

1. Integration with 3rd party HMI
2. Integration with 3rd party interfaces to install software on secondary ECUs
3. Limit network traffic and installation to specific vehicle states
4. Progress bars

Non-functional Requirements
===========================

1. Since in-vehicle interfaces are often proprietary and under NDA, the
   implementation must be kept separate from the \aktualizr tree.
2. It should be possible to have the integration performed by a separate team
3. Flexibility and simple integration into new environments.

Overview
========

\aktualizr performs all its operations in a background thread, and all the
operations except aktualizr_init() and aktualizr_free() are asynchronous.
Notifications occur via callbacks. These calls are made synchronously from
\aktualizr's background thread, so they should complete quickly. Normally the
callback will only post a notification the application's main event loop.


Configuration
=============
A configuration object is assembled by is built by calling
aktualizr_config_init() then loading configuration files or directories with
aktualizr_config_load(). After all the configuration options are loaded, the
configuration object is passed to aktualizr_init().

The configuration cannot be changed after it has been passed to
aktualizr_init()


Why not IPC?
============

It would be possible to allow other components to interface with \aktualizr via
a IPC interface such as D-Bus or Sockets. This would have the some advantages,
such as:

 - Process Isolation
 - Easier to assemble pieces together and hack together things with scripts.
 - Easier privilege separation

However we believe the disadvantages outweigh them:

- Have to build an IPC mechanism
- Have to build bridges to connect this 'common' IPC mechanism to whatever the
  end-user system has picked
- New features may require simultaneous changes to the aktualizr IPC these
  bridges.
- Hard to replace built-in functionality with external functionality (e.g.
  Package Managers)
- Harder to add steps to the high-level workflow.

It is still possible to write a wrapper that would expose libaktualizr via an
IPC mechanism, but this can be written with a specific use-case and IPC
technology in mind.

*/

#ifdef __cplusplus
extern "C" {
#endif

struct aktualizr_struct;

/**
 * \aktualizr handle
 */
typedef aktualizr_struct* aktualizr_t;

struct aktualizr_config_struct;

/** \aktualizr configuration object handle */
typedef aktualizr_config_struct* aktualizr_config_t;

/**
 * \aktualizr API result code.
 */
typedef enum {
  /** The operation completed successfully */
  AKTUALIZR_OK = 0,
  /** The operation cannot be started because an existing operation is in progress */
  AKTUALIZR_ALREADY_IN_PROGRESS,
  /** The operation cannot be performed with the current \aktualizr configuration */
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
 * Future, finer grained categories will be exposed via new enumerations, not
 * be extending this one.
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
} aktualizr_update_ui_category_t;

#define AKTUALIZR_UI_STATUS_MAX_LEN 300

/**
 * A snapshot of the current update state
 */
struct aktualizr_update_state {
  /**
   * The high-level category of this update state snapshot.
   */
  aktualizr_update_ui_category_t ui_category;

  /**
   * A one-line description of the update suitable for display to the user.
   * This might be the filename, or a short description of the package suitable
   * for display like "Downloading %s".
   */
  char ui_status_line[AKTUALIZR_UI_STATUS_MAX_LEN];
  /** \private */
  double padding1;
  /** \private */
  long long padding2;
  /** \private */
  int padding3;
  /** \private */
  int padding4;
  // Don't add any more fields to this struct: it will break ABI compatibility!
  // Instead either take over one of the padding fields or define a new
  // structure and a new sibling to aktualizr_set_state_change_callback
};

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

/**
 * Callback type for \ref aktualizr_set_state_change_callback
 * update_state is owned by the caller, and must be copied out if it will be
 * used beyond the end of this callback.
 */
typedef void (*aktualizr_callback_t)(void* clientp, struct aktualizr_update_state* update_state);

///
/// \name Main API
/// \{

/**
 * Create a new \aktualizr software updater.
 * If polling is enabled in the configuration, this will start polling the
 * update server in the background.
 */
aktualizr_t aktualizr_init(aktualizr_config_t configuration);

/**
 * Shutdown \aktualizr. No other calls to this API may be made after this call.
 */
void aktualizr_free(aktualizr_t aktualizr);

/**
 * Register to be notified when updates are available, downloading or installing.
 * It is not possible to register to multiple callbacks. Passing NULL as a
 * callback will disable state change notifications.
 * \param aktualizr \aktualizr handle
 * \param callback The callback to call, or 'NULL' to disable notifications
 * \param userdata Opaque user handle that is passed to the callback
 */
void aktualizr_set_state_change_callback(aktualizr_t aktualizr, aktualizr_callback_t callback, void* userdata);

/**
 * Read the current state of the update process into update_state.
 */
aktualizr_res_t aktualizr_get_update_state(aktualizr_t aktualizr, struct aktualizr_update_state* update_state);

/**
 * Perform an update cycle.
 * If \aktualizr isn't currently idle (for example if a polling operation is
 * already in progress) then \ref AKTUALIZR_ALREADY_IN_PROGRESS is returned.
 */
aktualizr_res_t aktualizr_run_update(aktualizr_t aktualizr, aktualizr_poll_operation_t operation);

/// \name \aktualizr Configuration
/// \{

/**
 * Create a new configuration object, which is used to build \aktualizr's
 * configuration.
 */
aktualizr_config_t aktualizr_config_init();

/**
 * Free an \aktualizr configuration object.
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

///
/// \}
///

#ifdef __cplusplus
}
#endif

#endif  // AKTUALIZR_API_H_