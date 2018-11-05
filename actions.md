## libaktualizr

### External actions

These are the primary actions that a user of libaktualizr can perform through the API.

- [x] Initialization
  - [ ] Set boot count to 0 to indicate successful boot
  - [x] Initialize secondaries
    - [x] Discover secondaries over TCP-IP (ipsecondary_discovery_test.cc)
    - [x] Add secondaries from configuration (uptane_test.cc)
      - [x] Parse secondary config files in JSON format (config_test.cc)
      - [x] Create secondary object
        - [x] Create a virtual secondary for testing (uptane_secondary_test.cc)
    - [ ] Add secondaries via API
    - [ ] Adding multiple secondaries with the same serial throws an error
  - [x] Initialize device ID
    - [x] Use a provided device ID (OTA-985, uptane_init_test.cc)
    - [x] Generate a random device ID (OTA-986, utils_test.cc, uptane_init_test.cc)
  - [x] Provision with the server
    - [x] Automatically provision (OTA-983, uptane_init_test.cc, uptane_ci_test.cc, auto_prov_test.py)
      - [x] Extract credentials from a provided archive (config_test.cc, utils_test.cc)
      - [x] Parse a p12 file containing TLS credentials (crypto_test.cc)
      - [x] aktualizr possesses all necessary credentials after provisioning (OTA-987, uptane_key_test.cc)
    - [x] Implicitly provision (OTA-996, OTA-1210, config_test.cc, uptane_implicit_test.cc, uptane_test.cc, implicit_prov_test.py)
      - [x] Fail if TLS credentials are unavailable (OTA-1209, uptane_implicit_test.cc)
    - [x] Implicitly provision with keys accessed via PKCS#11 (hsm_prov_test.py)
      - [x] Generate RSA keypairs via PKCS#11 (crypto_test.cc, keymanager_test.cc)
      - [x] Read a TLS certificate via PKCS#11 (crypto_test.cc)
      - [x] Sign and verify a file with RSA via PKCS#11 (crypto_test.cc, keymanager_test.cc)
  - [x] Initialize primary ECU keys
    - [x] Generate primary ECU keys (OTA-989, uptane_serial_test.cc)
      - [x] Generate RSA 2048 key pairs (crypto_test.cc)
      - [x] Generate RSA 4096 key pairs (crypto_test.cc)
      - [x] Generate ED25519 key pairs (crypto_test.cc)
  - [x] Initialize primary ECU serial
    - [x] Use a provided primary serial (OTA-988, config_test.cc)
    - [x] Generate primary serial (OTA-989, uptane_serial_test.cc)
    - [ ] Use a provided hardware ID
    - [ ] Use the system hostname as hardware ID if one is not provided
      - [x] Read the hostname from the system (utils_test.cc)
  - [x] Register ECUs with director (uptane_test.cc)
  - [x] Abort if initialization fails
    - [x] Recover from partial provisioning and network loss (OTA-991, uptane_network_test.cc, uptane_key_test.cc)
    - [x] Detect and recover from failed provisioning (uptane_init_test.cc)
  - [ ] Verify secondaries against storage
    - [ ] Identify previously unknown secondaries
    - [ ] Identify currently unavailable secondaries
- [x] Send system/network info to server
  - [x] Read hardware info from the system (utils_test.cc)
  - [x] Send hardware info to the server (OTA-984, uptane_test.cc)
  - [x] Import a list of installed packages into an SQL database (uptane_test.cc)
    - [x] Store a list of installed package versions (uptane_test.cc)
  - [x] Send a list of installed packages to the server (OTA-984, uptane_test.cc)
  - [x] Read networking info from the system (utils_test.cc)
  - [x] Send networking info to the server (OTA-984, uptane_test.cc)
  - [x] Generate and send manifest (see below)
  - [ ] Send SendDeviceDataComplete event
- [x] Check for campaigns
  - [x] Check for campaigns with manual control (aktualizr_test.cc)
  - [x] Fetch campaigns from the server (aktualizr_test.cc)
  - [x] Parse campaigns from JSON (campaign_test.cc)
  - [ ] Send CampaignCheckComplete event with campaign data
- [ ] Accept a campaign
  - [ ] Send campaign acceptance report
    - [x] Send an event report (see below)
  - [ ] Send CampaignAcceptComplete event
- [x] Fetch metadata from server
  - [x] Generate and send manifest (see below)
  - [x] Fetch metadata from the director (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Check metadata from the director (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Identify targets for known ECUs (uptane_test.cc, uptane_vector_tests.cc)
  - [ ] Ignore updates for unrecognized ECUs
  - [x] Fetch metadata from the images repo (uptane_test.cc, uptane_vector_tests.cc)
  - [x] Check metadata from the images repo (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
- [x] Check for updates
  - [x] Check metadata from the director (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Identify updates for known ECUs (uptane_test.cc, uptane_vector_tests.cc)
  - [ ] Ignore updates for unrecognized ECUs
  - [x] Check metadata from the images repo (uptane_test.cc, uptane_vector_tests.cc)
    - [x] Validate Uptane metadata (see below)
  - [x] Send UpdateCheckComplete event with available updates (aktualizr_test.cc)
  - [x] Send UpdateCheckComplete event after successful check with no available updates (aktualizr_test.cc)
  - [ ] Send UpdateCheckComplete event after failure
- [x] Download updates
  - [x] Download an update
    - [ ] Download an OSTree package
    - [x] Download a binary package (uptane_vector_tests.cc, aktualizr_test.cc)
    - [x] Send EcuDownloadStartedReport to server (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [ ] Report download progress
  - [x] Pause downloading (fetcher_test.cc)
    - [ ] Pausing while paused is ignored
    - [ ] Pausing while not downloading is ignored
  - [x] Resume downloading (fetcher_test.cc)
    - [ ] Resuming while not paused is ignored
    - [ ] Resuming while not downloading is ignored
  - [x] Verify a downloaded update
    - [ ] Verify an OSTree package
    - [x] Verify a binary package (uptane_vector_tests.cc, aktualizr_test.cc)
    - [x] Send EcuDownloadCompletedReport to server (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Send DownloadTargetComplete event if download is successful (aktualizr_test.cc)
  - [ ] Send DownloadTargetComplete event if download is partially successful
  - [ ] Send DownloadTargetComplete event if there is nothing to download
  - [ ] Send DownloadTargetComplete event if download is unsuccessful
  - [x] Send AllDownloadsComplete after all downloads are finished (aktualizr_test.cc)
- [x] Access downloaded binaries via API (aktualizr_test.cc)
- [x] Install updates
  - [ ] Send metadata to secondary ECUs
  - [x] Identify ECU for each target (uptane_test.cc, aktualizr_test.cc)
    - [ ] Reject targets which do not match a known ECU
  - [x] Install updates on primary
    - [x] Check if there are updates to install for the primary (uptane_test.cc, aktualizr_test.cc)
    - [ ] Check if an update is already installed
    - [ ] Set boot count to 0 and rollback flag to 0 to indicate system update
    - [x] Send InstallStarted event for primary (aktualizr_test.cc)
    - [x] Send EcuInstallationStartedReport to server for primary (uptane_test.cc, aktualizr_test.cc)
      - [x] Send an event report (see below)
    - [x] Install an update on the primary
      - [ ] Install an OSTree update on the primary
      - [x] Install a binary update on the primary (uptane_test.cc, aktualizr_test.cc)
    - [x] Store installation result for primary (uptane_test.cc)
    - [x] Send InstallTargetComplete event for primary (aktualizr_test.cc)
    - [x] Send EcuInstallationCompletedReport to server for primary (uptane_test.cc, aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Install updates on secondaries
    - [x] Send InstallStarted event for secondaries (aktualizr_test.cc)
    - [ ] Send EcuInstallationStartedReport to server for secondaries
      - [x] Send an event report (see below)
    - [x] Send images to secondary ECUs (aktualizr_test.cc)
    - [x] Send InstallTargetComplete event for secondaries (aktualizr_test.cc)
    - [x] Send EcuInstallationCompletedReport to server for secondaries (aktualizr_test.cc)
      - [x] Send an event report (see below)
  - [x] Send AllInstallsComplete event after all installations are finished (aktualizr_test.cc)
- [x] Send installation report
  - [x] Generate and send manifest (see below)
  - [x] send PutManifestComplete event if send is successful (aktualizr_test.cc)
  - [ ] Send PutManifestComplete event if send is unsuccessful

### Internal requirements

These are internal requirements that are relatively opaque to the user and/or common to multiple external actions.

- [x] Validate Uptane metadata
  - [x] Validate SHA256 hashes (crypto_test.cc)
  - [x] Validate SHA512 hashes (crypto_test.cc)
  - [x] Sign and verify a file with RSA stored in a file (crypto_test.cc)
  - [x] Reject bad signatures (crypto_test.cc)
  - [x] Verify an ED25519 signature (crypto_test.cc)
  - [x] Sign TUF metadata (keymanager_test.cc)
  - [x] Validate a TUF root (tuf_test.cc, uptane_test.cc)
  - [x] Throw an exception if a TUF root is invalid (tuf_test.cc, uptane_test.cc)
  - [x] Parse Uptane timestamps (tuf_test.cc)
  - [x] Throw an exception if an Uptane timestamp is invalid (tuf_test.cc)
  - [x] Recover from an interrupted Uptane iteration (uptane_test.cc)
  - [x] Verify Uptane metadata while offline (uptane_test.cc)
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
  - [x] Abort update if the director targets metadata has an invalid ECU ID (uptane_vector_tests.cc)
- [x] Generate and send manifest
  - [x] Get manifest from primary (uptane_test.cc)
  - [x] TODO Get primary installation result
  - [x] Get manifest from secondaries (uptane_test.cc)
  - [x] TODO Get secondary installation result
  - [x] Send manifest to the server (uptane_test.cc)
- [x] Send an event report
  - [x] Generate a random UUID (utils_test.cc)
  - [x] Include correlation ID from targets metadata (aktualizr_test.cc)
    - [x] Correlation ID is empty if none was provided in targets metadata (aktualizr_test.cc)
  - [x] Report an event to the server (reportqueue_test.cc)
  - [x] Report a series of events to the server (reportqueue_test.cc)
  - [x] Recover from errors while sending event reports (reportqueue_test.cc)
- [x] Support OSTree as a package manager (packagemanagerfactory_test.cc, ostreemanager_test.cc)
  - [x] Reject bad OSTree server URIs (ostreemanager_test.cc)
  - [x] Abort if the OSTree sysroot is invalid (ostreemanager_test.cc)
  - [x] Parse a provided list of installed packages (ostreemanager_test.cc)
  - [x] Communicate with a remote OSTree server (ostreemanager_test.cc)
- [x] Store state in an SQL database
  - [x] Migrate forward through SQL schemas (sqlstorage_test.cc)
  - [x] Reject invalid SQL databases (sqlstorage_test.cc)
  - [x] Migrate from the legacy filesystem storage (sqlstorage_test.cc, uptane_test.cc)
  - [x] Load and store primary keys in an SQL database (storage_common_test.cc)
  - [x] Load and store TLS credentials in an SQL database (storage_common_test.cc)
  - [x] Load and store Uptane metadata in an SQL database (storage_common_test.cc)
  - [x] Load and store the device ID in an SQL database (storage_common_test.cc)
  - [x] Load and store ECU serials in an SQL database (storage_common_test.cc)
  - [x] Load and store a list of misconfigured ECUs in an SQL database (storage_common_test.cc)
  - [x] Load and store a flag indicating successful registration in an SQL database (storage_common_test.cc)
  - [x] Load and store an installation result in an SQL database (storage_common_test.cc)
  - [x] Load and store targets in an SQL database (storage_common_test.cc)
  - [x] Import keys and credentials from file into an SQL database (storage_common_test.cc)
- [x] Configuration
  - [x] Parse config files in TOML format (config_test.cc)
  - [x] Write its config to file or to the log (config_test.cc)
  - [x] Parse multiple config files in a directory (config_test.cc)
  - [x] Parse multiple config files in multiple directories (config_test.cc)
- [ ] Miscellaneous
  - [x] Create a temporary file (utils_test.cc)
  - [x] Create a temporary directory (utils_test.cc)
  - [x] Serialize and deserialize asn1 (asn1_test.cc)
  - [x] Support a fake package manager for testing (packagemanagerfactory_test.cc)
  - [x] Install a fake package for testing (uptane_test.cc)
  - [x] ~~Support a Debian package manager~~ (packagemanagerfactory_test.cc, debianmanager_test.cc)
  - [x] TODO Update secondaries (aktualizr_test.cc)
  - [x] Support virtual partial verification secondaries for testing
    - [x] Partial verification secondaries generate and store public keys (uptane_secondary_test.cc)
    - [x] Partial verification secondaries can verify Uptane metadata. (uptane_secondary_test.cc)
  - [x] Support OPC-UA secondaries (opcuabridge_messaging_test.cc, opcuabridge_secondary_update_test.cc, run_opcuabridge_ostree_repo_sync_test.sh)

## High-level sequence tests

  - [x] Do nothing further in automatic mode if there are no updates to install (aktualizr_test.cc)
  - [x] Download with manual control (aktualizr_test.cc)
  - [x] Do not download automatically with manual control (aktualizr_test.cc)
  - [x] Perform a complete Uptane cycle with automatic control (aktualizr_test.cc)
  - [x] Install with manual control (aktualizr_test.cc)
  - [x] Do not install automatically with manual control (aktualizr_test.cc)

## aktualizr-primary

`aktualizr-primary` is the reference user of the libaktualizr API. Note that for historical reasons, it is usually simply known as `aktualizr`.

- [x] Abort when given bogus command line options (tests/CMakeLists.txt)
- [x] Abort when given a nonexistant config file (tests/CMakeLists.txt)
- [x] Support debug logging (tests/CMakeLists.txt)
- [x] Default to informational logging (tests/CMakeLists.txt)

## aktualizr-secondary

`aktualizr-secondary` was designed to demonstrate an Uptane-compliant secondary but is currently not part of the core product.

- [x] Parse config files in TOML format (aktualizr_secondary_config_test.cc)
- [x] Write its config to file or to the log (aktualizr_secondary_config_test.cc)
- [x] Announce itself to aktualizr primary (aktualizr_secondary_discovery_test.cc)

- [x] Generate a random serial (aktualizr_secondary/uptane_test.cc)
- [x] TODO Generate a hardware ID (aktualizr_secondary/uptane_test.cc)
- [x] Generate keys (aktualizr_secondary/uptane_test.cc)
- [x] Extract credentials from a provided archive (aktualizr_secondary/uptane_test.cc)

- [x] Abort when given bogus command line options (aktualizr_secondary/CMakeLists.txt)
- [x] Abort when given a nonexistant config file (aktualizr_secondary/CMakeLists.txt)
- [x] Support debug logging (aktualizr_secondary/CMakeLists.txt)
- [x] Default to informational logging (aktualizr_secondary/CMakeLists.txt)

## aktualizr-info

`aktualizr-info` is designed to provide information to a developer with access to a device running aktualizr.

- [x] Parse config files in TOML format (aktualizr_info_config_test.cc)
- [x] Write its config to file or to the log (aktualizr_info_config_test.cc)
- [x] Print information about aktualizr (run_aktualizr_info_tests.sh)

## aktualizr-repo

`aktualizr-repo` is used in testing to simulate the generation of Uptane repositories.

- [x] Generate images and director repos (repo_test.cc)
- [x] Add an image to the images repo (repo_test.cc)
- [x] Copy an image to the director repo (repo_test.cc)

## garage-push

`garage-push` pushes OSTree objects to a remote Treehub server.

- [x] Extract credentials from a provided archive (authenticate_test.cc)
- [x] Extract credentials from a provided JSON file (authenticate_test.cc)
- [x] Authenticate with TLS credentials (authenticate_test.cc)
- [x] Communicate with a server that does not require authentication (authenticate_test.cc)
- [x] Parse OSTree hashes (ostree_hash_test.cc)
- [x] Push concurrently (rate_controller_test.cc)

- [x] Abort when given bogus command line options (sota_tools/CMakeLists.txt)
- [x] Abort when given a bogus OSTree repo (sota_tools/CMakeLists.txt)
- [x] Abort when given a bogus OSTree ref (sota_tools/CMakeLists.txt)
- [x] Abort when given expired metadata (sota_tools/CMakeLists.txt)
- [x] Abort when the server is unresponsive after three tries (sota_tools/CMakeLists.txt)
- [x] Recover from intermittent errors (sota_tools/CMakeLists.txt)
- [x] Support debug logging (sota_tools/CMakeLists.txt)

## garage-deploy

`garage-deploy` moves OSTree objects from one remote Treehub server to another.

- [x] Extract credentials from a provided archive (authenticate_test.cc)
- [x] Extract credentials from a provided JSON file (authenticate_test.cc)
- [x] Authenticate with TLS credentials (authenticate_test.cc)
- [x] Communicate with a server that does not require authentication (authenticate_test.cc)
- [x] Parse OSTree hashes (ostree_hash_test.cc)
- [x] Push concurrently (rate_controller_test.cc)

- [x] Abort when given bogus command line options (sota_tools/CMakeLists.txt)
- [ ] Abort when given a bogus OSTree repo (sota_tools/CMakeLists.txt)
- [ ] Abort when given a bogus OSTree ref (sota_tools/CMakeLists.txt)
- [ ] Abort when given expired metadata (sota_tools/CMakeLists.txt)
- [ ] Abort when the server is unresponsive after three tries (sota_tools/CMakeLists.txt)
- [ ] Recover from intermittent errors (sota_tools/CMakeLists.txt)
- [ ] Support debug logging (sota_tools/CMakeLists.txt)

- [ ] Offline sign with garage-sign
- [ ] Do not reuse lingering credentials from previous runs of garage-sign

## garage-check

`garage-check` simply verifies that a given OSTree commit exists on a remote Treehub server.

- [ ] Verify that a commit exists in a remote repo

## cert-provider

`cert-provider` assists with generating credentials and uploading them to a device.

- [ ] Copy credentials to a device
- [ ] Generate a fleet root CA

## meta-updater

`meta-updater` is a Yocto layer used to build aktualizr and OSTree.

- [x] Run garage-push
- [x] Run garage-deploy
- [x] Run garage-sign
- [x] Build credentials into an image
- [x] Run aktualizr_cert_provider
- [x] Build an image with automatic provisioning that provisions successfully
- [x] Build an image with implicit provisioning that provisions successfully
- [x] Build an image with implicit provisioning using an HSM that provisions successfully
- [x] Build an image with manual control that provisions successfully
- [x] Build an image for Raspberry Pi
- [x] Build an image using grub as a bootloader that provisions successfully
- [x] Build an image for a secondary
- [x] Build an image that listens for discovery
- [x] Build an image for a primary intended to connect to a secondary

