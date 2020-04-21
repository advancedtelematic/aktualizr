## libaktualizr

### External actions

These are the primary actions that a user of libaktualizr can perform through the API.

- [x] Initialization
  - [ ] Set boot count to 0 to indicate successful boot
  - [x] Detect a system reboot on the Primary, if expected (bootloader_test.cc)
  - [x] Initialize Secondaries
    - [x] Add Secondaries from configuration (uptane_test.cc)
      - [x] Parse Secondary config files in JSON format (config_test.cc)
      - [x] Create Secondary object
        - [x] Create a virtual Secondary for testing (uptane_secondary_test.cc)
    - [x] Add Secondaries via API (aktualizr_test.cc)
    - [x] Adding multiple Secondaries with the same serial throws an error (uptane_test.cc)
  - [x] Initialize device ID
    - [x] Use a provided device ID (OTA-985, uptane_init_test.cc)
    - [x] Generate a random device ID (OTA-986, utils_test.cc, uptane_init_test.cc)
  - [x] Finalize a pending update that requires reboot (aktualizr_test.cc)
    - [x] Store installation result (aktualizr_test.cc)
    - [x] Send manifest (aktualizr_test.cc)
    - [x] Update is not in pending state anymore after successful finalization (aktualizr_test.cc)
    - [x] Update is not in pending state anymore after failed finalization (aktualizr_test.cc)
  - [x] Provision with the server
    - [x] Provision with shared credentials (OTA-983, uptane_init_test.cc, uptane_ci_test.cc, shared_cred_prov_test.py)
      - [x] Extract credentials from a provided archive (config_test.cc, utils_test.cc)
      - [x] Parse a p12 file containing TLS credentials (crypto_test.cc)
      - [x] aktualizr possesses all necessary credentials after provisioning (OTA-987, uptane_key_test.cc)
    - [x] Provision with device credentials (OTA-996, OTA-1210, config_test.cc, device_cred_prov_test.cc, uptane_test.cc, device_cred_prov_test.py)
      - [x] Fail if TLS credentials are unavailable (OTA-1209, device_cred_prov_test.cc)
    - [x] Provision with device credentials using keys accessed via PKCS#11 (device_cred_prov_hsm_test.py)
      - [x] Generate RSA keypairs via PKCS#11 (crypto_test.cc, keymanager_test.cc)
      - [x] Read a TLS certificate via PKCS#11 (crypto_test.cc)
      - [x] Sign and verify a file with RSA via PKCS#11 (crypto_test.cc, keymanager_test.cc)
  - [x] Initialize Primary ECU keys
    - [x] Generate Primary ECU keys (OTA-989, uptane_serial_test.cc)
      - [x] Generate RSA 2048 key pairs (crypto_test.cc)
      - [x] Generate RSA 4096 key pairs (crypto_test.cc)
      - [x] Generate ED25519 key pairs (crypto_test.cc)
  - [x] Initialize Primary ECU serial
    - [x] Use a provided Primary serial (OTA-988, config_test.cc)
    - [x] Generate Primary serial (OTA-989, uptane_serial_test.cc)
    - [x] Use a provided hardware ID (uptane_test.cc)
    - [x] Use the system hostname as hardware ID if one is not provided (uptane_init_test.cc)
      - [x] Read the hostname from the system (utils_test.cc)
  - [x] Register ECUs with Director
    - [x] Register Primary ECU with Director (uptane_test.cc)
    - [x] Register Secondary ECUs with Director (uptane_test.cc)
  - [x] Abort if initialization fails
    - [x] Recover from partial provisioning and network loss (OTA-991, uptane_network_test.cc, uptane_key_test.cc)
    - [x] Detect and recover from failed provisioning (uptane_init_test.cc)
  - [x] Verify Secondaries against storage
    - [x] Identify previously unknown Secondaries (uptane_test.cc)
    - [x] Identify currently unavailable Secondaries (uptane_test.cc)
- [x] Send system/network info to server
  - [x] Read hardware info from the system (utils_test.cc)
  - [x] Send hardware info to the server (OTA-984, uptane_test.cc)
  - [x] Import a list of installed packages into the storage (uptane_test.cc)
    - [x] Store a list of installed package versions (uptane_test.cc)
  - [x] Send a list of installed packages to the server (OTA-984, uptane_test.cc)
  - [x] Read networking info from the system (utils_test.cc)
  - [x] Send networking info to the server (OTA-984, uptane_test.cc)
  - [x] Generate and send manifest (see below)
  - [x] Send SendDeviceDataComplete event (uptane_test.cc)
- [x] Check for campaigns
  - [x] Check for campaigns with manual control (aktualizr_test.cc)
  - [x] Fetch campaigns from the server (aktualizr_test.cc)
  - [x] Parse campaigns from JSON (campaign_test.cc)
  - [x] Send CampaignCheckComplete event with campaign data (aktualizr_test.cc)
- [x] Accept a campaign
  - [x] Send campaign acceptance report
    - [x] Send an event report (see below)
  - [x] Send CampaignAcceptComplete event
- [x] Decline a campaign
  - [x] Send campaign decline report
    - [x] Send an event report (see below)
  - [x] Send CampaignDeclineComplete event
- [x] Postpone a campaign
  - [x] Send campaign postpone report
    - [x] Send an event report (see below)
  - [x] Send CampaignPostponeComplete event
- [x] Fetch metadata from server
  - [x] Generate and send manifest (see below)
  - [x] Fetch metadata from the Director (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Check metadata from the Director (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Identify targets for known ECUs (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Ignore updates for unrecognized ECUs (uptane_test.cc uptane_vector_tests.cc)
  - [x] Fetch metadata from the Image repo (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Check metadata from the Image repo (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
- [x] Check for updates
  - [x] Check metadata from the Director (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Identify updates for known ECUs (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Ignore updates for unrecognized ECUs (uptane_test.cc uptane_vector_tests.cc)
  - [x] Check metadata from the Image repo (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Send UpdateCheckComplete event with available updates (aktualizr_test.cc)
  - [x] Send UpdateCheckComplete event after successful check with no available updates (aktualizr_test.cc)
  - [x] Send UpdateCheckComplete event after failure (aktualizr_test.cc)
- [x] Pause and Resume the API
  - [x] Suspend API calls during pause
  - [x] Catch up with calls queue after resume
- [x] Download updates
  - [x] Find requested target
    - [x] Search first-order delegations (uptane_delegation_test.cc)
  - [x] Download an update
    - [x] Download an OSTree package (fetcher_test.cc)
    - [x] Download a binary package (uptane_vector_tests.cc, aktualizr_test.cc)
    - [x] Download from URI specified in target metadata (fetcher_test.cc)
    - [x] Download from default file server URL (fetcher_test.cc)
    - [x] Send EcuDownloadStartedReport to server (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Report download progress (aktualizr_test.cc)
  - [x] Pause downloading (fetcher_test.cc)
    - [x] Pausing while paused is ignored (fetcher_test.cc)
    - [x] Pausing while not downloading is ignored (fetcher_test.cc)
  - [x] Resume downloading (fetcher_test.cc)
    - [x] Resuming while not paused is ignored (fetcher_test.cc)
    - [x] Resuming while not downloading is ignored (fetcher_test.cc)
    - [x] Resume download interrupted by restart (fetcher_test.cc)
  - [x] Verify a downloaded update
    - [x] Verify an OSTree package (fetcher_test.cc)
    - [x] Verify a binary package (uptane_vector_tests.cc, aktualizr_test.cc)
    - [x] Send EcuDownloadCompletedReport to server (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Send DownloadTargetComplete event if download is successful (aktualizr_test.cc)
  - [x] Send DownloadTargetComplete event if download is unsuccessful (aktualizr_test.cc)
  - [x] Send AllDownloadsComplete event after all downloads are finished (aktualizr_test.cc)
  - [x] Send AllDownloadsComplete event after partial download success (aktualizr_test.cc)
  - [x] Send AllDownloadsComplete event if there is nothing to download (aktualizr_test.cc)
  - [x] Send AllDownloadsComplete event if all downloads are unsuccessful (aktualizr_test.cc)
- [x] Access downloaded binaries via API (aktualizr_test.cc)
  - [x] List targets in storage via API (aktualizr_test.cc)
  - [x] Remove targets in storage via API(aktualizr_test.cc)
- [x] Install updates
  - [x] Send metadata to Secondary ECUs (uptane_test.cc)
  - [x] Identify ECU for each target (uptane_test.cc, aktualizr_test.cc)
    - [x] Reject targets which do not match a known ECU (uptane_test.cc)
  - [x] Install updates on Primary
    - [x] Check if there are updates to install for the Primary (uptane_test.cc, aktualizr_test.cc)
    - [x] Check if an update is already installed (uptane_test.cc)
    - [ ] Set boot count to 0 and rollback flag to 0 to indicate system update
    - [x] Send InstallStarted event for Primary (aktualizr_test.cc)
    - [x] Send EcuInstallationStartedReport to server for Primary (uptane_test.cc, aktualizr_test.cc)
      - [x] Send an event report (see below)
    - [x] Install an update on the Primary
      - [x] Install an OSTree update on the Primary (aktualizr_fullostree_test.cc)
      - [ ] Notify "reboot needed" after an OSTree update trigger
      - [x] Set new version to pending status after an OSTree update trigger (aktualizr_test.cc)
      - [x] Send EcuInstallationAppliedReport to server after an OSTree update trigger (aktualizr_test.cc)
      - [x] Uptane check for updates and manifest sends are disabled while an installation is pending reboot (aktualizr_test.cc)
      - [ ] Trigger a system reboot at the end of the installation process in case of the ostree package manager usage (OTA-2135)
      - [x] Emulate a reboot at the end of the installation process in case of the fake package manager usage (OTA-2135, aktualizr_test.cc)
      - [x] Install a binary update on the Primary (uptane_test.cc, aktualizr_test.cc)
    - [x] Store installation result for Primary (uptane_test.cc)
    - [x] Send InstallTargetComplete event for Primary (aktualizr_test.cc)
    - [x] Send EcuInstallationCompletedReport to server for Primary (uptane_test.cc, aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Install updates on Secondaries
    - [x] Send InstallStarted event for Secondaries (aktualizr_test.cc)
    - [x] Send EcuInstallationStartedReport to server for Secondaries (uptane_test.cc)
      - [x] Send an event report (see below)
    - [x] Send images to Secondary ECUs (aktualizr_test.cc)
    - [x] Store installation result for Secondary (aktualizr_test.cc)
    - [x] Send InstallTargetComplete event for Secondaries (aktualizr_test.cc)
    - [x] Send EcuInstallationCompletedReport to server for Secondaries (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Store installation result for device (uptane_test.cc)
  - [x] Compute device installation failure code as concatenation of ECU failure codes (aktualizr_test.cc)
  - [x] Store negative device installation result when an ECU installation failed (aktualizr_test.cc)
  - [x] Update is not in pending state anymore after failed installation (aktualizr_test.cc)
  - [x] Send AllInstallsComplete event after all installations are finished (aktualizr_test.cc)
  - [x] Automatically remove old targets during installation cycles (aktualizr_test.cc)
- [x] Send installation report
  - [x] Generate and send manifest (see below)
  - [x] Send PutManifestComplete event if send is successful (aktualizr_test.cc)
  - [x] Send PutManifestComplete event if send is unsuccessful (uptane_test.cc)
  - [x] Append custom installation report to the "custom" field of the manifest (aktualizr_test.cc)

### Internal and common actions

These are internal requirements that are relatively opaque to the user and/or common to multiple external actions.

- [x] Validate Uptane metadata
  - [x] Validate hashes
    - [x] Validate SHA256 hashes (crypto_test.cc)
    - [x] Validate SHA512 hashes (crypto_test.cc)
  - [x] Sign and verify signatures
    - [x] Sign and verify a file with RSA key stored in a file (crypto_test.cc)
    - [x] Verify an ED25519 signature (crypto_test.cc)
    - [x] Refuse to sign with an invalid key (crypto_test.cc)
    - [x] Reject a signature if the key is invalid (crypto_test.cc)
    - [x] Reject bad signatures (crypto_test.cc)
  - [x] Sign TUF metadata
    - [x] Sign TUF metadata with RSA2048 (keymanager_test.cc)
    - [x] Sign TUF metadata with ED25519 (keymanager_test.cc)
  - [x] Validate TUF roles (tuf_test.cc)
    - [x] Delegated roles have custom names (tuf_test.cc)
    - [x] Reject delegated role names that are identical to reserved role names (tuf_test.cc)
  - [x] Validate Root metadata (tuf_test.cc, uptane_test.cc)
    - [x] Throw an exception if Root metadata is invalid
      - [x] Throw an exception if Root metadata is unsigned (tuf_test.cc, uptane_test.cc)
      - [x] Throw an exception if Root metadata has no roles (tuf_test.cc)
      - [x] Throw an exception if Root metadata has unknown signature types (uptane_test.cc)
      - [x] Throw an exception if Root metadata has invalid key IDs (uptane_test.cc)
      - [x] Throw an exception if Root metadata signature threshold is invalid (uptane_test.cc)
  - [x] Parse Uptane timestamps (types_test.cc)
    - [x] Throw an exception if an Uptane timestamp is invalid (types_test.cc)
    - [x] Get current time (types_test.cc)
  - [x] Validate first-order target delegations (uptane_delegation_test.cc)
  - [x] Reject http GET responses that exceed size limit (httpclient_test.cc)
  - [x] Reject http GET responses that do not meet speed limit (httpclient_test.cc)
  - [x] Abort update if any signature threshold is <= 0 (REQ-153, uptane_vector_tests.cc)
  - [x] Abort update if any metadata has expired (REQ-150, uptane_vector_tests.cc)
  - [x] Abort update if a target hash is invalid (uptane_vector_tests.cc)
  - [x] Abort update if any signature threshold is unmet (uptane_vector_tests.cc)
  - [x] Abort update if any signatures are not unique (uptane_vector_tests.cc)
  - [x] Abort update if any metadata is unsigned (uptane_vector_tests.cc)
  - [x] Abort update if any metadata has an invalid key ID (uptane_vector_tests.cc)
  - [x] Abort update if a target is smaller than stated in the metadata (uptane_vector_tests.cc)
  - [x] Accept update with rotated Uptane roots (uptane_vector_tests.cc)
  - [x] Abort update with incorrectly rotated Uptane roots (uptane_vector_tests.cc)
  - [x] Abort update if any metadata has an invalid hardware ID (uptane_vector_tests.cc)
  - [x] Abort update if the Director Targets metadata has an invalid ECU ID (uptane_vector_tests.cc)
  - [x] Recover from an interrupted Uptane iteration (uptane_test.cc)
- [x] Generate and send manifest
  - [x] Get manifest from Primary (uptane_test.cc)
    - [x] Get Primary installation result (uptane_test.cc)
  - [x] Get manifest from Secondaries (uptane_test.cc)
    - [x] Ignore Secondaries with bad signatures (uptane_test.cc)
  - [x] Send manifest to the server (uptane_test.cc)
- [x] Send an event report
  - [x] Generate a random UUID (utils_test.cc)
  - [x] Include correlation ID from Targets metadata (aktualizr_test.cc)
    - [x] Correlation ID is empty if none was provided in Targets metadata (aktualizr_test.cc)
  - [x] Report an event to the server (reportqueue_test.cc)
    - [x] Report a series of events to the server (reportqueue_test.cc)
    - [x] Recover from errors while sending event reports (reportqueue_test.cc)
- [x] Support OSTree as a package manager (packagemanagerfactory_test.cc)
  - [x] Reject bad OSTree server URIs (ostreemanager_test.cc)
  - [x] Abort if the OSTree sysroot is invalid (ostreemanager_test.cc)
  - [x] Parse a provided list of installed packages (ostreemanager_test.cc)
  - [x] Communicate with a remote OSTree server
    - [x] Communicate with a remote OSTree server without credentials (ostreemanager_test.cc)
    - [x] Communicate with a remote OSTree server with credentials (ostreemanager_test.cc)
- [x] Store state in an SQL database
  - [x] Migrate forward through SQL schemas (sqlstorage_test.cc)
    - [x] Automatically use latest SQL schema version when initializing database (sqlstorage_test.cc)
  - [x] Migrate backward through SQL schemas (sqlstorage_test.cc)
  - [x] Reject invalid SQL databases (sqlstorage_test.cc)
  - [x] Migrate from the legacy filesystem storage (sqlstorage_test.cc, uptane_test.cc)
  - [x] Load and store Primary keys in an SQL database (storage_common_test.cc)
  - [x] Load and store TLS credentials in an SQL database (storage_common_test.cc)
  - [x] Load and store Uptane metadata in an SQL database (storage_common_test.cc)
  - [x] Load and store Uptane roots in an SQL database (storage_common_test.cc)
  - [x] Load and store the device ID in an SQL database (storage_common_test.cc)
  - [x] Load and store ECU serials in an SQL database (storage_common_test.cc)
    - [x] Preserve ECU ordering between store and load calls (storage_common_test.cc)
  - [x] Load and store a list of misconfigured ECUs in an SQL database (storage_common_test.cc)
  - [x] Load and store a flag indicating successful registration in an SQL database (storage_common_test.cc)
  - [x] Load and store an ecu installation result in an SQL database (storage_common_test.cc)
  - [x] Load and store a device installation result in an SQL database (storage_common_test.cc)
  - [x] Load and store targets in an SQL database (storage_common_test.cc)
  - [x] Import keys and credentials from file into an SQL database (storage_common_test.cc)
- [x] Configuration
  - [x] Parse config files in TOML format (config_test.cc)
  - [x] Write config to file or to the log (config_test.cc)
  - [x] Parse multiple config files in a directory (config_test.cc)
  - [x] Parse multiple config files in multiple directories (config_test.cc)
- [x] Miscellaneous
  - [x] Create a temporary file (utils_test.cc)
    - [x] Write to a temporary file (utils_test.cc)
  - [x] Create a temporary directory (utils_test.cc)
  - [x] Serialize and deserialize asn1 (asn1_test.cc)
  - [x] Support a fake package manager for testing (packagemanagerfactory_test.cc)
  - [x] ~~Support a Debian package manager~~ (packagemanagerfactory_test.cc, debianmanager_test.cc)
  - [x] Support virtual partial verification Secondaries for testing
    - [x] Partial verification Secondaries generate and store public keys (uptane_secondary_test.cc)
    - [x] Partial verification Secondaries can verify Uptane metadata (uptane_secondary_test.cc)

### Expected action sequences

This is just the list of sequences currently covered. It is likely that there are more worth testing, but these tests are expensive.

- [x] Initialize -> UptaneCycle -> no updates -> no further action or events (aktualizr_test.cc)
- [x] Initialize -> UptaneCycle -> updates downloaded and installed for Primary and Secondary (aktualizr_test.cc)
- [x] Initialize -> UptaneCycle -> updates downloaded and installed for Primary (after reboot) and Secondary (aktualizr_test.cc)
- [x] Initialize -> UptaneCycle -> updates downloaded and installed for Secondaries without changing the Primary (aktualizr_test.cc)
- [x] Initialize -> CheckUpdates -> no updates -> no further action or events (aktualizr_test.cc)
- [x] Initialize -> Download -> nothing to download (aktualizr_test.cc)
- [x] Initialize -> CheckUpdates -> Download -> updates downloaded but not installed (aktualizr_test.cc)
- [x] Initialize -> Install -> nothing to install (aktualizr_test.cc)
- [x] Initialize -> CheckUpdates -> Download -> Install -> updates installed (aktualizr_test.cc)
- [x] Provision with shared credentials with real server. Initialize -> CheckUpdates -> verify state with aktualizr-info (shared_cred_prov_test.py)
- [x] Provision with device credentials with real server. Initialize -> verify not provisioned with aktualizr-info -> run aktualizr-cert-provider -> Initialize -> CheckUpdates -> verify state with aktualizr-info (device_cred_prov_test.py)
- [x] Provision with device credentials with real server using an HSM. Initialize -> verify not provisioned with aktualizr-info -> run aktualizr-cert-provider -> Initialize -> CheckUpdates -> verify state with aktualizr-info (device_cred_prov_hsm_test.py)


## aktualizr tools

These tools all link with libaktualizr, although they do not necessary use the API.

### aktualizr-primary

`aktualizr-primary` is the reference user of the libaktualizr API. Note that for historical reasons, it is usually simply known as `aktualizr`. It is a thin layer around libaktualizr. This is just the list of things currently tested that relate specifically to it.

- [x] Abort when given bogus command line options (tests/CMakeLists.txt)
- [x] Abort when given a nonexistant config file (tests/CMakeLists.txt)
- [x] Support debug logging (tests/CMakeLists.txt)
- [x] Default to informational logging (tests/CMakeLists.txt)

### aktualizr-secondary

`aktualizr-secondary` was designed to demonstrate an Uptane-compliant Secondary but is currently not part of the core product. It also uses libaktualizr, but less extensively than `aktualizr-primary`. This is just the list of things currently tested that relate specifically to it.

- [x] Parse config files in TOML format (aktualizr_secondary_config_test.cc)
- [x] Write its config to file or to the log (aktualizr_secondary_config_test.cc)

- [x] Generate a serial (aktualizr_secondary/uptane_test.cc)
- [x] Generate a hardware ID (aktualizr_secondary/uptane_test.cc)
- [x] Generate keys (aktualizr_secondary/uptane_test.cc)
- [x] Extract credentials from a provided archive (aktualizr_secondary/uptane_test.cc)

- [x] Abort when given bogus command line options (aktualizr_secondary/CMakeLists.txt)
- [x] Abort when given a nonexistant config file (aktualizr_secondary/CMakeLists.txt)
- [x] Support debug logging (aktualizr_secondary/CMakeLists.txt)
- [x] Default to informational logging (aktualizr_secondary/CMakeLists.txt)

### aktualizr-info

`aktualizr-info` provides information about libaktualizr's state to a developer with access to a device.

- [x] Parse libaktualizr configuration files (see Configuration section above)
  - [x] Parse config files in TOML format (aktualizr_info_config_test.cc)
  - [x] Write its config to file or to the log (aktualizr_info_config_test.cc)
- [x] Print information from libaktualizr storage (aktualizr_info_test.cc)
  - [x] Print device ID (aktualizr_info_test.cc)
  - [x] Print Primary ECU serial (aktualizr_info_test.cc)
  - [x] Print Primary ECU hardware ID (aktualizr_info_test.cc)
  - [x] Print Secondary ECU serials (aktualizr_info_test.cc)
  - [x] Print Secondary ECU hardware IDs (aktualizr_info_test.cc)
  - [x] Print Secondary ECUs no longer accessible (miscofigured: old) (aktualizr_info_test.cc)
  - [x] Print Secondary ECUs registered after provisioning (not registered) (aktualizr_info_test.cc)
  - [x] Print provisioning status (aktualizr_info_test.cc)
  - [x] Print whether metadata has been fetched from the server (aktualizr_info_test.cc)
  - [x] Print Root metadata from Image repository (aktualizr_info_test.cc)
  - [x] Print Targets metadata from Image repository (aktualizr_info_test.cc)
  - [x] Print Root metadata from Director repository (aktualizr_info_test.cc)
  - [x] Print Targets metadata from Director repository (aktualizr_info_test.cc)
  - [x] Print TLS credentials (aktualizr_info_test.cc)
  - [x] Print Primary ECU keys (aktualizr_info_test.cc)
  - [x] Print Primary ECU current and pending versions (aktualizr_info_test.cc)
  - [x] Print Secondary ECU current and pending versions (aktualizr_info_test.cc)
  - [x] Print device name only for scripting purposes (aktualizr_info_test.cc)
  - [x] Print delegations (aktualizr_info_test.cc)
  - [x] Print Snapshot metadata (aktualizr_info_test.cc)
  - [x] Print Timestamp metadata (aktualizr_info_test.cc)

### uptane-generator

`uptane-generator` is used in testing to simulate the generation of Uptane repositories.

- [x] Generate Image and Director repos (repo_test.cc)
- [x] Add an image to the Image repo (repo_test.cc)
  - [x] Add custom image metadata without an actual file (repo_test.cc)
- [x] Copy an image to the Director repo (repo_test.cc)
  - [x] Clear the staged Director Targets metadata (repo_test.cc)
  - [x] Populate the Director Targets metadata with the currently signed metadata (repo_test.cc)
- [x] Sign Director repo Targets metadata (repo_test.cc)
- [x] Add simple delegation (repo_test.cc)
- [x] Add image with delegation (repo_test.cc)
- [x] Sign arbitrary metadata (repo_test.cc)

### aktualizr-cert-provider

`aktualizr-cert-provider` assists with generating credentials and uploading them to a device for device credential provisioning.

- [x] Use file paths from config if provided (cert_provider_test.cc)
- [x] Use shared provisioning credentials if fleet CA and private key are not provided (cert_provider_shared_cred_test.cc)
  - [x] Generate a random device ID (OTA-986, utils_test.cc, uptane_init_test.cc)
  - [x] Provision with shared credentials (cert_provider_shared_cred_test.cc)
- [x] Use fleet credentials if provided (cert_provider_test.cc)
  - [x] Abort if fleet CA is provided without fleet private key (cert_provider_test.cc)
  - [x] Abort if fleet private key is provided without fleet CA (cert_provider_test.cc)
  - [x] Specify RSA bit length (cert_provider_test.cc)
  - [x] Specify device certificate expiration date (cert_provider_test.cc)
  - [x] Specify device certificate country code (cert_provider_test.cc)
  - [x] Specify device certificate state abbreviation (cert_provider_test.cc)
  - [x] Specify device certificate organization name (cert_provider_test.cc)
  - [x] Specify device certificate common name (cert_provider_test.cc)
  - [x] Read fleet CA certificate (cert_provider_test.cc)
  - [x] Read fleet private key (cert_provider_test.cc)
  - [x] Create device certificate (cert_provider_test.cc)
  - [x] Create device keys (cert_provider_test.cc)
  - [x] Set public key for the device certificate (cert_provider_test.cc)
  - [x] Sign device certificate with fleet private key (cert_provider_test.cc)
  - [x] Serialize device private key to a string (cert_provider_test.cc)
  - [x] Serialize device certificate to a string (cert_provider_test.cc)
- [ ] Read server root CA from credentials archive
  - [ ] Read server root CA from server_ca.pem if present (to support community edition use case)
  - [x] Read server root CA from p12 (cert_provider_shared_cred_test.cc)
- [x] Write credentials to a local directory if requested (cert_provider_test.cc)
  - [x] Provide device private key (cert_provider_test.cc)
  - [x] Provide device certificate (cert_provider_test.cc)
  - [x] Provide root CA if requested (cert_provider_shared_cred_test.cc)
  - [x] Provide server URL if requested (cert_provider_shared_cred_test.cc)
- [ ] Copy credentials to a device with ssh
  - [ ] Create parent directories
  - [ ] Provide device private key
  - [ ] Provide device certificate
  - [ ] Provide root CA if requested
  - [ ] Provide server URL if requested


## Garage (sota) tools

These tools also use libaktualizr, but only for common utility functions. They are focused specifically on dealing with OSTree objects. They originally lived in a separate repo, which is where the "sota_tools" name came from. The garage nomenclature refers to the historical name of our reference SaaS server, ATS Garage, before it was renamed HERE OTA Connect.

### garage-push

`garage-push` pushes OSTree objects to a remote Treehub server.

- [x] Verify a local OSTree repository (ostree_dir_repo_test.cc)
  - [x] Reject invalid path (ostree_dir_repo_test.cc)
  - [x] Reject invalid repo configuration (ostree_dir_repo_test.cc)
  - [x] Reject bare mode repo (ostree_dir_repo_test.cc)
- [x] Parse credentials (see below)
- [x] Parse OSTree ref or commit refhash (sota_tools/CMakeLists.txt, test-dry-run)
  - [x] Find OSTree commit object in local repository (see below)
  - [x] Generate an OSTree hash from a commit ref (see below)
- [x] Authenticate with treehub server (see below)
- [x] Fetch OSTree objects from source repository and push to destination repository (see below)
- [x] Check if credentials support offline signing (authenticate_test.cc)
- [ ] Upload root ref to Image repository if credentials do not support offline signing
- [x] Abort when given bogus command line options (sota_tools/CMakeLists.txt, test-bad-option)
- [x] Support debug logging (sota_tools/CMakeLists.txt, test-verbose-logging)

### garage-deploy

`garage-deploy` moves OSTree objects from one remote Treehub server to another.

- [x] Parse credentials for destination server (see below)
- [x] Parse credentials for source server (see below)
- [x] Authenticate with source server (see below)
- [x] Generate an OSTree hash from a commit ref (see below)
- [x] Fetch OSTree objects from source repository and push to destination repository (see below)
  - [x] Abort if commit is not present in source server (sota_tools/CMakeLists.txt, test-missing-commit)
- [x] Check if credentials support offline signing (authenticate_test.cc)
  - [x] Abort if credentials do not support offline signing (sota_tools/CMakeLists.txt, test-garage-deploy-online-signing)
- [x] Use garage-sign to offline sign targets for destination repository (sota_tools/CMakeLists.txt, test-garage-deploy-offline-signing)
  - [ ] Do not reuse lingering credentials from previous runs of garage-sign
  - [x] Remove local tuf repo generated by garage-sign after use (sota_tools/CMakeLists.txt, test-garage-deploy-offline-signing)
- [x] Abort when given bogus command line options (sota_tools/CMakeLists.txt, test-bad-option)
- [x] Support debug logging (sota_tools/CMakeLists.txt)
- [x] Support trace logging (sota_tools/CMakeLists.txt)

### garage-check

`garage-check` simply verifies that a given OSTree commit exists on a remote Treehub server and is present in the targets.json from the Image repository.

- [x] Parse credentials (see below)
- [x] Authenticate with treehub server (see below)
- [x] Verify that a commit exists in a remote repo (sota_tools/CMakeLists.txt, run_expired_test.sh)
- [x] Get targets.json from Image repository (sota_tools/CMakeLists.txt, run_expired_test.sh)
  - [x] Abort if targets.json has expired (sota_tools/CMakeLists.txt, run_expired_test.sh)
- [x] Find specified OSTree ref in targets.json (sota_tools/CMakeLists.txt, run_expired_test.sh)

### Internal and common actions

- [x] Parse credentials
  - [x] Reject a bogus provided file (authenticate_test.cc)
    - [x] Abort when given nonexistent credentials (sota_tools/CMakeLists.txt, test-missing-credentials)
    - [x] Abort when given bogus credentials (sota_tools/CMakeLists.txt, test-invalid-credentials, test-garage-deploy-missing-fetch-credentials, test-garage-deploy-missing-push-credentials)
  - [x] Extract credentials from a provided archive (authenticate_test.cc)
    - [x] Reject a provided archive file without a treehub.json (authenticate_test.cc)
    - [x] Reject a provided archive file with bogus credentials (authenticate_test.cc)
  - [x] Extract credentials from a provided JSON file (authenticate_test.cc)
    - [x] Reject a bogus provided JSON file (authenticate_test.cc)
  - [x] Parse authentication information from treehub.json (authenticate_test.cc)
  - [x] Parse Image repository URL from a provided archive (authenticate_test.cc)
  - [ ] Parse treehub URL from a provided archive
- [x] Authenticate with treehub server
  - [x] Authenticate with username and password (basic auth) (treehub_server_test.cc)
  - [x] Authenticate with OAuth2 (treehub_server_test.cc, authenticate_test.cc)
  - [ ] Authenticate with TLS credentials (authenticate_test.cc [BROKEN])
  - [x] Authenticate with nothing (no auth) (authenticate_test.cc)
  - [x] Use a provided CA certificate (sota_tools/CMakeLists.txt, test-cacert-used)
    - [x] Abort when given a bogus CA certificate (sota_tools/CMakeLists.txt, test-cacert-not-found)
  - [x] Abort if authorization fails (sota_tools/CMakeLists.txt, test-auth-plus-failure)
  - [x] Abort if destination server is unavailable (sota_tools/CMakeLists.txt, test-garage-deploy-upload-failed)
- [x] Generate an OSTree hash from a ref string
  - [x] Generate an OSTree hash from a ref string (ostree_hash_test.cc)
  - [x] Ignore case of OSTree ref strings (ostree_hash_test.cc)
  - [x] Reject empty OSTree ref strings (ostree_hash_test.cc)
  - [x] Reject bogus OSTree ref strings (ostree_hash_test.cc)
- [x] Fetch OSTree objects from source repository and push to destination repository (deploy_test.cc)
  - [x] Get OSTree commit object in source repository (see below)
  - [x] Query destination repository for OSTree commit object (see below)
  - [x] Parse OSTree object to identify child objects (deploy_test.cc)
  - [x] Query destination repository for child objects recursively (see below)
  - [x] Get child objects from source repository recursively (see below)
  - [x] Upload missing OSTree objects to destination repository (ostree_object_test.cc)
    - [x] Detect curl misconfiguration (ostree_object_test.cc)
    - [x] Skip upload if dry run was specified (ostree_object_test.cc)
      - [x] Support dry run with local repos (sota_tools/CMakeLists.txt, test-garage-deploy-dry-run)
      - [x] Support dry run with auth plus using a real server (sota_tools/CMakeLists.txt, test-dry-run)
    - [x] Upload objects concurrently (rate_controller_test.cc)
      - [x] Initial rate controller status is good (rate_controller_test.cc)
      - [x] Rate controller aborts if it detects server or network failure (rate_controller_test.cc)
      - [x] Rate controller continues through intermittent errors (rate_controller_test.cc)
      - [x] Rate controller improves concurrency when network conditions are good (rate_controller_test.cc)
    - [x] Recover from the server hanging on to connections (sota_tools/CMakeLists.txt, test-server-500)
    - [x] Recover from intermittent errors (sota_tools/CMakeLists.txt, test-server-error_every_10)
    - [x] Abort when server becomes unresponsive (sota_tools/CMakeLists.txt, test-server-500_after_20)
- [x] Get OSTree object from a repository
  - [x] Find OSTree object in local repository (ostree_dir_repo_test.cc)
    - [x] Check all valid OSTree object extensions (ostree_dir_repo_test.cc)
    - [x] Abort if OSTree object is not found (ostree_dir_repo_test.cc)
      - [x] Abort when given a bogus OSTree ref (sota_tools/CMakeLists.txt, test-missing-ref)
  - [x] Fetch OSTree object from remote repository (ostree_http_repo.cc)
    - [x] Check all valid OSTree object extensions (ostree_http_repo.cc)
    - [x] Retry fetch if not found after first try (ostree_http_repo.cc)
    - [x] Abort if OSTree object is not found after retry (ostree_http_repo.cc)
- [x] Query destination repository for OSTree object (ostree_object_test.cc)
  - [x] Expect HTTP 200 for a hash that we expect the server to know about (ostree_object_test.cc)
  - [x] Expect HTTP 404 for a hash that we expect the server not to know about (ostree_object_test.cc)


## meta-updater

`meta-updater` is a Yocto layer used to build aktualizr, garage-push, OSTree, and other related tools.

- [x] Run garage-push
- [x] Run garage-deploy
- [x] Run garage-sign
- [x] Build credentials into an image
- [x] Run aktualizr-cert-provider
- [x] Build an image with shared credential provisioning that provisions successfully
- [x] Build an image with device credential provisioning that provisions successfully
- [x] Build an image with device credential provisioning using an HSM that provisions successfully
- [x] Build an image with manual control that provisions successfully
- [x] Build an image for Raspberry Pi
- [x] Build an image using grub as a bootloader that provisions successfully
- [x] Build an image for a Secondary
- [x] Build an image for a Primary intended to connect to a Secondary

