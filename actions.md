## libaktualizr

### External actions

These are the primary actions that a user of libaktualizr can perform through the API.

- [x] Provisioning
  - [x] Initialization
    - [x] aktualizr can use a provided primary serial (OTA-988, config_test.cc)
    - [x] aktualizr can generate primary serial and keys (OTA-989, uptane_serial_test.cc)
      - [x] aktualizr can generate RSA 2048 key pairs (crypto_test.cc)
      - [x] aktualizr can generate RSA 4096 key pairs (crypto_test.cc)
      - [x] aktualizr can generate ED25519 key pairs (crypto_test.cc)
    - [x] aktualizr can use a provided device ID (OTA-985, uptane_init_test.cc)
    - [x] aktualizr can generate a random device ID (OTA-986, utils_test.cc, uptane_init_test.cc)
    - [ ] TODO aktualizr can use a provided hardware ID
    - [ ] TODO aktualizr uses the system hostname as hardware ID if one is not provided
      - [x] aktualizr can read the hostname from the system (utils_test.cc)
  - [x] aktualizr can automatically provision (OTA-983, uptane_init_test.cc, uptane_ci_test.cc, auto_prov_test.py)
    - [x] aktualizr can extract credentials from a provided archive (config_test.cc, utils_test.cc)
    - [x] aktualizr can parse a p12 file containing TLS credentials (crypto_test.cc)
    - [x] aktualizr possesses all necessary credentials after provisioning (OTA-987, uptane_key_test.cc)
    - [x] aktualizr can recover from partial provisioning and network loss (OTA-991, uptane_network_test.cc, uptane_key_test.cc)
  - [x] aktualizr can implicitly provision (OTA-996, OTA-1210, config_test.cc, uptane_implicit_test.cc, uptane_test.cc, implicit_prov_test.py)
    - [x] aktualizr fails if it does not have TLS credentials (OTA-1209, uptane_implicit_test.cc)
  - [x] aktualizr can implicitly provision with keys accessed via PKCS#11 (hsm_prov_test.py)
    - [x] aktualizr can generate RSA keypairs via PKCS#11 (crypto_test.cc)
    - [x] aktualizr can read a TLS certificate via PKCS#11 (crypto_test.cc)
    - [x] aktualizr can generate RSA keys with PKCS#11 (keymanager_test.cc)
    - [x] aktualizr can sign and verify a file with RSA via PKCS#11 (crypto_test.cc)
- [x] Send system/network info to server
  - [x] aktualizr can read networking info from the system (utils_test.cc)
  - [x] aktualizr can send networking info to the server (OTA-984, uptane_test.cc)
  - [x] aktualizr can import a list of installed packages into an SQL database (uptane_test.cc)
  - [x] aktualizr can load and store a list of installed package versions (uptane_test.cc)
  - [x] aktualizr can send a list of installed packages to the server (OTA-984, uptane_test.cc)
- [x] Fetch metadata from server
  - [x] aktualizr can assemble a manifest (uptane_test.cc)
  - [x] aktualizr can send a manifest to the server (uptane_test.cc)
  - [ ] TODO aktualizr can fetch metadata from the director
  - [ ] TODO aktualizr identifies targets for known ECUs
  - [ ] TODO aktualizr ignores updates for unrecognized ECUs
  - [ ] TODO aktualizr can fetch metadata from the images repo
- [x] Check for campaigns
  - [x] aktualizr can check for campaigns with manual control (aktualizr_test.cc)
  - [x] aktualizr can parse campaigns from JSON (campaign_test.cc)
- [ ] Accept campaigns
- [x] Check for updates
  - [x] aktualizr does nothing if there are no updates to install (aktualizr_test.cc)
  - [x] aktualizr can validate SHA256 hashes (crypto_test.cc)
  - [x] aktualizr can validate SHA512 hashes (crypto_test.cc)
  - [x] aktualizr can sign and verify a file with RSA stored in a file (crypto_test.cc)
  - [x] aktualizr rejects bad signatures (crypto_test.cc)
  - [x] aktualizr can verify an ED25519 signature (crypto_test.cc)
  - [x] aktualizr can sign TUF metadata (keymanager_test.cc)
  - [x] aktualizr can validate a TUF root (tuf_test.cc, uptane_test.cc)
  - [x] aktualizr throws an exception if a TUF root is invalid (tuf_test.cc, uptane_test.cc)
  - [x] aktualizr can parse Uptane timestamps (tuf_test.cc)
  - [x] aktualizr throws an except if an Uptane timestamp is invalid (tuf_test.cc)
  - [x] aktualizr can recover from an interrupted Uptane iteration (uptane_test.cc)
  - [x] aktualizr can verify Uptane metadata while offline (uptane_test.cc)
  - [x] aktualizr rejects http GET responses that exceed size limit (httpclient_test.cc)
  - [x] aktualizr rejects http GET responses that do not meet speed limit (httpclient_test.cc)
  - [x] aktualizr aborts update if any signature threshold is <= 0 (REQ-153, uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any metadata has expired (REQ-150, uptane_vector_tests.cc)
  - [x] aktualizr aborts update if a target hash is invalid (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any signature threshold is unmet (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any signatures are not unique (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any metadata is unsigned (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any metadata has an invalid key ID (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if a target is smaller than stated in the metadata (uptane_vector_tests.cc)
  - [x] aktualizr can update with rotated Uptane roots (uptane_vector_tests.cc)
  - [x] aktualizr aborts update with incorrectly rotated Uptane roots (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if any metadata has an invalid hardware ID (uptane_vector_tests.cc)
  - [x] aktualizr aborts update if the director targets metadata has an invalid ECU ID (uptane_vector_tests.cc)
- [x] Download updates
  - [x] aktualizr can download with manual control (aktualizr_test.cc)
  - [x] aktualizr does not download automatically with manual control (aktualizr_test.cc)
- [x] Install updates
  - [x] aktualizr can perform a complete Uptane cycle with automatic control (aktualizr_test.cc)
  - [x] aktualizr can install with manual control (aktualizr_test.cc)
  - [x] aktualizr does not install automatically with manual control (aktualizr_test.cc)
- [x] Send installation report
  - [x] aktualizr can assemble a manifest (uptane_test.cc)
  - [x] aktualizr can send a manifest to the server (uptane_test.cc)
- [x] Send event reports
  - [x] aktualizr can report an event to the server (reportqueue_test.cc)
  - [x] aktualizr can report a series of events to the server (reportqueue_test.cc)
  - [x] aktualizr can recover from errors while sending event reports (reportqueue_test.cc)

### Internal requirements

These are internal requirements that are relatively opaque to the user and possibly common to multiple external actions.

- [x] aktualizr supports OSTree as a package manager (packagemanagerfactory_test.cc, ostreemanager_test.cc)
  - [x] aktualizr rejects bad OSTree server URIs (ostreemanager_test.cc)
  - [x] aktualizr aborts if the OSTree sysroot is invalid (ostreemanager_test.cc)
  - [x] aktualizr can parse a provided list of installed packages (ostreemanager_test.cc)
  - [x] aktualizr can communicate with a remote OSTree server (ostreemanager_test.cc)
- [x] aktualizr can update secondaries (aktualizr_test.cc)
  - [x] aktualizr discover secondaries over TCP-IP (ipsecondary_discovery_test.cc)
  - [x] aktualizr supports virtual secondaries for testing (uptane_secondary_test.cc)
  - [x] aktualizr supports virtual Uptane secondaries for testing (uptane_secondary_test.cc)
  - [x] aktualizr can install a fake package for testing (uptane_test.cc)
  - [x] aktualizr can add secondaries (uptane_test.cc)
  - [x] (?) aktualizr supports OPC-UA (opcuabridge_messaging_test.cc, opcuabridge_secondary_update_test.cc, run_opcuabridge_ostree_repo_sync_test.sh)
- [x] aktualizr stores its state in an SQL database
  - [x] aktualizr can migrate forward through SQL schemas (sqlstorage_test.cc)
  - [x] aktualizr rejects invalid SQL databases (sqlstorage_test.cc)
  - [x] aktualizr can migrate from the legacy filesystem storage (sqlstorage_test.cc, uptane_test.cc)
  - [x] aktualizr can load and store primary keys in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store TLS credentials in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store Uptane metadata in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store the device ID in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store ECU serials in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store a list of misconfigured ECUs in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store a flag indicating successful registration in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store an installation result in an SQL database (storage_common_test.cc)
  - [x] aktualizr can load and store targets in an SQL database (storage_common_test.cc)
  - [x] aktualizr can import keys and credentials from file into an SQL database (storage_common_test.cc)
- [x] Configuration
  - [x] aktualizr can parse config files in TOML format (config_test.cc)
  - [x] aktualizr can parse secondary config files in JSON format (config_test.cc)
  - [x] aktualizr can write its config to file or to the log (config_test.cc)
  - [x] aktualizr can parse multiple config files in a directory (config_test.cc)
  - [x] aktualizr can parse multiple config files in multiple directories (config_test.cc)
- [x] Host system interaction
  - [x] aktualizr can generate a random UUID (utils_test.cc)
  - [x] aktualizr can create a temporary file (utils_test.cc)
  - [x] aktualizr can create a temporary directory (utils_test.cc)
- [ ] Miscellaneous
  - [x] aktualizr can convert Events to and from JSON (events_test.cc)
  - [x] aktualizr can serialize and deserialize asn1 (asn1_test.cc)
  - [ ] aktualizr can reset bootcount and rollback flag
  - [x] aktualizr has a fake package manager for testing (packagemanagerfactory_test.cc)
  - [x] ~~aktualizr has a Debian package manager~~ (packagemanagerfactory_test.cc, debianmanager_test.cc)

## aktualizr-primary

`aktualizr-primary` is the reference user of the libaktualizr API. Note that for historical reasons, it is usually simply known as `aktualizr`.

- [x] aktualizr-primary aborts when given bogus command line options (tests/CMakeLists.txt)
- [x] aktualizr-primary aborts when given a nonexistant config file (tests/CMakeLists.txt)
- [x] aktualizr-primary supports debug logging (tests/CMakeLists.txt)
- [x] aktualizr-primary defaults to informational logging (tests/CMakeLists.txt)

## aktualizr-secondary

`aktualizr-secondary` was designed to demonstrate an Uptane-compliant secondary but is currently not part of the core product.

- [x] aktualizr-secondary can parse config files in TOML format (aktualizr_secondary_config_test.cc)
- [x] aktualizr-secondary can write its config to file or to the log (aktualizr_secondary_config_test.cc)
- [x] aktualizr-secondary announce itself to aktualizr primary (aktualizr_secondary_discovery_test.cc)

- [x] aktualizr-secondary can generate a random serial (aktualizr_secondary/uptane_test.cc)
- [x] aktualizr-secondary can generate a hardware ID (?) (aktualizr_secondary/uptane_test.cc)
- [x] aktualizr-secondary can generate keys (aktualizr_secondary/uptane_test.cc)
- [x] aktualizr-secondary can extract credentials from a provided archive (aktualizr_secondary/uptane_test.cc)

- [x] aktualizr-secondary aborts when given bogus command line options (aktualizr_secondary/CMakeLists.txt)
- [x] aktualizr-secondary aborts when given a nonexistant config file (aktualizr_secondary/CMakeLists.txt)
- [x] aktualizr-secondary supports debug logging (aktualizr_secondary/CMakeLists.txt)
- [x] aktualizr-secondary defaults to informational logging (aktualizr_secondary/CMakeLists.txt)

## aktualizr-info

`aktualizr-info` is designed to provide information to a developer with access to a device running aktualizr.

- [x] aktualizr-info can parse config files in TOML format (aktualizr_info_config_test.cc)
- [x] aktualizr-info can write its config to file or to the log (aktualizr_info_config_test.cc)
- [x] aktualizr-info can print information about aktualizr (run_aktualizr_info_tests.sh)

## aktualizr-repo

`aktualizr-repo` is used in testing to simulate the generation of Uptane repositories.

- [x] aktualizr-repo can generate images and director repos (repo_test.cc)
- [x] aktualizr-repo can add an image to the images repo (repo_test.cc)
- [x] aktualizr-repo can copy an image to the director repo (repo_test.cc)

## garage-push

`garage-push` pushes OSTree objects to a remote Treehub server.

- [x] garage-push can extract credentials from a provided archive (authenticate_test.cc)
- [x] garage-push can extract credentials from a provided JSON file (authenticate_test.cc)
- [x] garage-push can authenticate with TLS credentials (authenticate_test.cc)
- [x] garage-push can communicate with a server that does not require authentication (authenticate_test.cc)
- [x] garage-push can parse OSTree hashes (ostree_hash_test.cc)
- [x] garage-push can push concurrently (rate_controller_test.cc)

- [x] garage-push aborts when given bogus command line options (sota_tools/CMakeLists.txt)
- [x] garage-push aborts when given a bogus OSTree repo (sota_tools/CMakeLists.txt)
- [x] garage-push aborts when given a bogus OSTree ref (sota_tools/CMakeLists.txt)
- [x] garage-push aborts when given expired metadata (sota_tools/CMakeLists.txt)
- [x] garage-push aborts when the server is unresponsive after three tries (sota_tools/CMakeLists.txt)
- [x] garage-push can recover from intermittent errors (sota_tools/CMakeLists.txt)
- [x] garage-push supports debug logging (sota_tools/CMakeLists.txt)

## garage-deploy

`garage-deploy` moves OSTree objects from one remote Treehub server to another.

- [x] garage-deploy can extract credentials from a provided archive (authenticate_test.cc)
- [x] garage-deploy can extract credentials from a provided JSON file (authenticate_test.cc)
- [x] garage-deploy can authenticate with TLS credentials (authenticate_test.cc)
- [x] garage-deploy can communicate with a server that does not require authentication (authenticate_test.cc)
- [x] garage-deploy can parse OSTree hashes (ostree_hash_test.cc)
- [x] garage-deploy can push concurrently (rate_controller_test.cc)

- [x] garage-deploy aborts when given bogus command line options (sota_tools/CMakeLists.txt)
- [ ] garage-deploy aborts when given a bogus OSTree repo (sota_tools/CMakeLists.txt)
- [ ] garage-deploy aborts when given a bogus OSTree ref (sota_tools/CMakeLists.txt)
- [ ] garage-deploy aborts when given expired metadata (sota_tools/CMakeLists.txt)
- [ ] garage-deploy aborts when the server is unresponsive after three tries (sota_tools/CMakeLists.txt)
- [ ] garage-deploy can recover from intermittent errors (sota_tools/CMakeLists.txt)
- [ ] garage-deploy supports debug logging (sota_tools/CMakeLists.txt)

- [ ] garage-deploy can offline sign with garage-sign
- [ ] garage-deploy does not reuse lingering credentials from previous runs of garage-sign

## garage-check

`garage-check` simply verifies that a given OSTree commit exists on a remote Treehub server.

- [ ] garage-check can verify that a commit exists in a remote repo

## cert-provider

`cert-provider` assists with generating credentials and uploading them to a device.

- [ ] cert-provider can copy credentials to a device
- [ ] cert-provider can generate a fleet root CA

## meta-updater

`meta-updater` is a Yocto layer used to build aktualizr and OSTree.

- [x] meta-updater can run garage-push
- [x] meta-updater can run garage-deploy
- [x] meta-updater can run garage-sign
- [x] meta-updater can build credentials into an image
- [x] meta-updater can run aktualizr_implicit_writer
- [x] meta-updater can run aktualizr_cert_provider
- [x] meta-updater can run aktualizr_cert_provider
- [x] meta-updater can build an image with automatic provisioning that provisions successfully
- [x] meta-updater can build an image with implicit provisioning that provisions successfully
- [x] meta-updater can build an image with implicit provisioning using an HSM that provisions successfully
- [x] meta-updater can build an image with manual control that provisions successfully
- [x] meta-updater can build an image for Raspberry Pi
- [x] meta-updater can build an image using grub as a bootloader that provisions successfully
- [x] meta-updater can build an image for a secondary
- [x] meta-updater can build an image that listens for discovery
- [x] meta-updater can build an image for a primary intended to connect to a secondary

