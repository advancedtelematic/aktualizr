# Changelog

This file summarizes notable changes introduced in aktualizr version. It roughly follows the guidelines from [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

Our versioning scheme is `YEAR.N` where `N` is incremented whenever a new release is deemed necessary. Thus it does not exactly map to months of the year.

## [??? (unreleased)]

## [2019.11] - 2019-12-12

### Added

- Allow logger to use stderr: [PR](https://github.com/advancedtelematic/aktualizr/pull/1457)
- Full metadata verification on IP Secondaries: [PR](https://github.com/advancedtelematic/aktualizr/pull/1449)
- Log when connectivity is restored after an interruption: [PR](https://github.com/advancedtelematic/aktualizr/pull/1463)
- Aktualizr now sends its configuration to the backend at boot, for audit purposes: [PR](https://github.com/advancedtelematic/aktualizr/pull/1474)

### Changed

- The jsoncpp library is now included as a submodule and was updated to v1.8.4: [PR](https://github.com/advancedtelematic/aktualizr/pull/1462)
- PKCS11 engine paths auto-detection is not done at runtime anymore, but at configure time when possible: [PR](https://github.com/advancedtelematic/aktualizr/pull/1471)

### Fixed

- Removed bogus warning at boot when using OStree: [PR](https://github.com/advancedtelematic/aktualizr/pull/1466)
- Updated the docker-app package manager to work with docker-app v0.8: [PR](https://github.com/advancedtelematic/aktualizr/pull/1468)
- Overriding of log level when using the docker-app package manager: [PR](https://github.com/advancedtelematic/aktualizr/pull/1478)
- Report correct hash of the currently installed version on IP Secondary:: [PR](https://github.com/advancedtelematic/aktualizr/pull/1485)

## [2019.10] - 2019-11-15

### Added

- Option to send Android repo manifest via garage-push: [PR](https://github.com/advancedtelematic/aktualizr/pull/1440)
- Expanded [C API](https://github.com/advancedtelematic/aktualizr/blob/master/include/libaktualizr-c.h): [PR #1387](https://github.com/advancedtelematic/aktualizr/pull/1387) and [PR #1429](https://github.com/advancedtelematic/aktualizr/pull/1429)

### Changed

- Hardware information is only sent if it has changed: [PR](https://github.com/advancedtelematic/aktualizr/pull/1434)
- Builds without OSTree now default to using the binary package manager: [PR](https://github.com/advancedtelematic/aktualizr/pull/1432)
- New endpoint for reporting hardware information: [PR](https://github.com/advancedtelematic/aktualizr/pull/1421)

### Removed

- libsystemd dependency and socket activation support: [PR](https://github.com/advancedtelematic/aktualizr/pull/1437)

### Fixed

- Enforce a limit of 10 HTTP redirects: [PR](https://github.com/advancedtelematic/aktualizr/pull/1420)
- Reject malformed root.json: [PR](https://github.com/advancedtelematic/aktualizr/pull/1417)
- Fall back on full file download if byte range requests are not supported: [PR](https://github.com/advancedtelematic/aktualizr/pull/1416)


## [2019.9] - 2019-10-16

### Added

- Handle POSIX signals: [PR](https://github.com/advancedtelematic/aktualizr/pull/1384)
- Store target custom metadata when installing: [PR](https://github.com/advancedtelematic/aktualizr/pull/1370)

### Fixed

- Incorrect installation status reported if installation interrupted: [PR](https://github.com/advancedtelematic/aktualizr/pull/1402)
- Binary updates of secondaries from an OSTree primary is again possible: [PR](https://github.com/advancedtelematic/aktualizr/pull/1395)
- Applications built from release tarballs now report a valid version: [PR](https://github.com/advancedtelematic/aktualizr/pull/1415)


## [2019.8] - 2019-09-12

### Fixed

- garage-deploy logic with checking for keys and verifying successful push: [PR](https://github.com/advancedtelematic/aktualizr/pull/1347)


## [2019.7] - 2019-09-10

### Added

- `GetInstallationLog` API method: [PR](https://github.com/advancedtelematic/aktualizr/pull/1318)
- The aktualizr daemon will now automatically remove old downloaded targets to free up disk space: [PR](https://github.com/advancedtelematic/aktualizr/pull/1318)
- CA path is now always supplied to curl and can be overwritten: [PR](https://github.com/advancedtelematic/aktualizr/pull/1294)

### Changed

- garage-push and garage-deploy can now stream OSTree objects to [S3 via Treehub](https://github.com/advancedtelematic/treehub/pull/70) (instead of getting copied): [PR](https://github.com/advancedtelematic/aktualizr/pull/1305)

### Removed

- hmi-stub (replaced by [libaktualizr-demo-app](https://github.com/advancedtelematic/libaktualizr-demo-app)): [PR](https://github.com/advancedtelematic/aktualizr/pull/1310)

### Fixed

- Uptane metadata is now rechecked (offline) before downloading and installing: [PR](https://github.com/advancedtelematic/aktualizr/pull/1296)
- Downloaded target hashes are rechecked before installation: [PR](https://github.com/advancedtelematic/aktualizr/pull/1296)
- Failed downloads are now reported to the backend in the installation report: [PR](https://github.com/advancedtelematic/aktualizr/pull/1301)
- Binary targets for an OSTree-based primary are now rejected immediately: [PR](https://github.com/advancedtelematic/aktualizr/pull/1282)


## [2019.6] - 2019-08-21

### Added

- garage-sign metadata expiration parameters: [PR](https://github.com/advancedtelematic/ota-tuf/pull/237)
- aktualizr-info --wait-until-provisioned flag: [PR](https://github.com/advancedtelematic/aktualizr/pull/1253)
- Target object equality requires that hardware IDs match: [PR](https://github.com/advancedtelematic/aktualizr/pull/1258)
- aktualizr-repo image command now requires a hardware ID: [PR](https://github.com/advancedtelematic/aktualizr/pull/1258)
- `GetStoredTargets` and `DeleteStoredTarget` aktualizr API methods: [PR](https://github.com/advancedtelematic/aktualizr/pull/1290)
- [aktualizr-get](https://github.com/advancedtelematic/aktualizr/blob/master/src/aktualizr_get/main.cc) debugging tool: [PR](https://github.com/advancedtelematic/aktualizr/pull/1276)
- Automatic reboot command is now customizable: [PR](https://github.com/advancedtelematic/aktualizr/pull/1274)
- Basic [C API](https://github.com/advancedtelematic/aktualizr/blob/master/include/libaktualizr-c.h): [PR](https://github.com/advancedtelematic/aktualizr/pull/1263)
- Ability to pass custom headers in HTTP requests: [PR](https://github.com/advancedtelematic/aktualizr/pull/1251)
- Mutual TLS support in garage tools: [PR #1243](https://github.com/advancedtelematic/aktualizr/pull/1243) and [PR #1288](https://github.com/advancedtelematic/aktualizr/pull/1288)

### Changed

- Renamed `GetStoredTarget` to `OpenStoredTarget` in aktualizr API: [PR](https://github.com/advancedtelematic/aktualizr/pull/1290)
- Renamed aktualizr-repo to uptane-generator: [PR](https://github.com/advancedtelematic/aktualizr/pull/1279)
- [Documentation](https://github.com/advancedtelematic/aktualizr/blob/master/docs/README.adoc) substantially restructed: [PR](https://github.com/advancedtelematic/aktualizr/pull/1293)
- Target matching between the Director and Image repositories is now done as early as possible during the check for updates: [PR](https://github.com/advancedtelematic/aktualizr/pull/1271)
- Target matching requires the hardware IDs to match: [PR](https://github.com/advancedtelematic/aktualizr/pull/1258)
- Custom URL logic now prefers the Director and if it is empty, only then checks the Image repository value: [PR](https://github.com/advancedtelematic/aktualizr/pull/1267)


## [2019.5] - 2019-07-12

### Added

- TLS support by aktualizr-lite: [PR](https://github.com/advancedtelematic/aktualizr/pull/1237)
- automatic garage-check usage at the end of garage-push/deploy: [PR](https://github.com/advancedtelematic/aktualizr/pull/1244)
- ccache support: [PR] (https://github.com/advancedtelematic/aktualizr/pull/1248, https://github.com/advancedtelematic/aktualizr/pull/1249)
- doc on Primary and Secondary bitbaking for RPi: [PR](https://github.com/advancedtelematic/aktualizr/pull/1238)

### Changed

- Stripping of executables in release mode: [PR](https://github.com/advancedtelematic/aktualizr/pull/1234)
- VirtualSecondary configuration: [PR](https://github.com/advancedtelematic/aktualizr/pull/1246)

### Removed

- Jenkins pipeline and a few references: [PR](https://github.com/advancedtelematic/aktualizr/pull/1236)
- Test repo meta: [PR](https://github.com/advancedtelematic/aktualizr/pull/1239)
- SecondaryFactory and VirtualSecondary out of libaktualizr: [PR](https://github.com/advancedtelematic/aktualizr/pull/1241)
- Fallback on clang-{tidy,format}: [PR](https://github.com/advancedtelematic/aktualizr/pull/1240)

### Fixed

- Logic of finding the latest version by aktualizr-lite: [PR](https://github.com/advancedtelematic/aktualizr/pull/1247)
- Test regression in docker-app-mgr: [PR] (https://github.com/advancedtelematic/aktualizr/pull/1250)
- Some more lintian fixes: [PR](https://github.com/advancedtelematic/aktualizr/pull/1242)


## [2019.4] - 2019-06-14

### Added

- Campaigns can be declined and postponed via the API: [PR](https://github.com/advancedtelematic/aktualizr/pull/1225)
- Warn when running two libaktualizr instances simultaneously: [PR #1217](https://github.com/advancedtelematic/aktualizr/pull/1217) and [PR #1229](https://github.com/advancedtelematic/aktualizr/pull/1229)
- aktualizr-info can output the Snapshot and Timestamp metadata from the Images repository: [PR](https://github.com/advancedtelematic/aktualizr/pull/1207)
- aktualizr-info can output the current and pending image versions for secondaries: [PR](https://github.com/advancedtelematic/aktualizr/pull/1201)
- [Support for docker-app package management on top of OSTree](src/libaktualizr/package_manager/dockerappmanager.h): [PR](https://github.com/advancedtelematic/aktualizr/pull/1189)

### Changed

- [Provisioning methods have been renamed](https://github.com/advancedtelematic/aktualizr/blob/master/docs/client-provisioning-methods.adoc). "Autoprovisioning" or "automatic provisioning" is now known as "shared credential provisioning". "Implicit provisioning" is now known as "device credential provisioning". "HSM provisioning" was always a misnomer, so it is now refered to as "device credential provisioning with an HSM". [PR# 1208](https://github.com/advancedtelematic/aktualizr/pull/1208) and [PR #1220](https://github.com/advancedtelematic/aktualizr/pull/1220)
- aktualizr-cert-provider is now included in the garage_deploy.deb releases: [PR](https://github.com/advancedtelematic/aktualizr/pull/1218)
- aktualizr-info metadata and key output is now printed without additional text for easier machine parsing (and piping to jq): [PR](https://github.com/advancedtelematic/aktualizr/pull/1215)
- The IP secondary implementation has been substantially refactored and improved with support for POSIX sockets and easier configuration: [PR #1183](https://github.com/advancedtelematic/aktualizr/pull/1183) and [PR #1198](https://github.com/advancedtelematic/aktualizr/pull/1198)

### Removed

- aktualizr-check-discovery (due to obsolescence): [PR](https://github.com/advancedtelematic/aktualizr/pull/1191)


## [2019.3] - 2019-04-29

### Added

- New tool aktualizr-lite for anonymous TUF-only updates: [PR](https://github.com/advancedtelematic/aktualizr/pull/1107)
- [Abort() API call](src/libaktualizr/primary/aktualizr.h): [PR](https://github.com/advancedtelematic/aktualizr/pull/1122)
- [Option to print delegation metadata with aktualizr-info](src/aktualizr_info/main.cc): [PR](https://github.com/advancedtelematic/aktualizr/pull/1138)
- Support for custom URIs for downloading targets: [PR](https://github.com/advancedtelematic/aktualizr/pull/1147)
- [SendManifest() API call](src/libaktualizr/primary/aktualizr.h): [PR](https://github.com/advancedtelematic/aktualizr/pull/1176)
- [Support for Android package management](src/libaktualizr/package_manager/androidmanager.h): [PR](https://github.com/advancedtelematic/aktualizr/pull/1034)

### Changed

- Device installation failure result codes are deduced as concatenation of ECU failure result codes: [PR](https://github.com/advancedtelematic/aktualizr/pull/1145)
- No longer require hashes and sizes of Tagets objects in Snapshot metadata: [PR](https://github.com/advancedtelematic/aktualizr/pull/1162)
- [Updated documentation](README.adoc): [PR](https://github.com/advancedtelematic/aktualizr/pull/1164)

### Removed

- example.com is no longer set as the default URL when using garage-sign via garage-deploy: [PR](https://github.com/advancedtelematic/aktualizr/pull/1169)
- OPC-UA secondary support: [PR](https://github.com/advancedtelematic/aktualizr/pull/1177)

### Fixed

- Check for updates even if sending the manifest fails: [PR](https://github.com/advancedtelematic/aktualizr/pull/1186)
- Correctly handle empty targets metadata: [PR #1186](https://github.com/advancedtelematic/aktualizr/pull/1186) and [PR #1192](https://github.com/advancedtelematic/aktualizr/pull/1192)
- Various OSTree-related memory leaks and suppressions: [PR #1114](https://github.com/advancedtelematic/aktualizr/pull/1114), [PR #1120](https://github.com/advancedtelematic/aktualizr/pull/1120), and [PR #1179](https://github.com/advancedtelematic/aktualizr/pull/1179)
- Various spurious and/or confusing log messages, e.g.: [PR #1112](https://github.com/advancedtelematic/aktualizr/pull/1112), [PR #1137](https://github.com/advancedtelematic/aktualizr/pull/1137), and [PR #1180](https://github.com/advancedtelematic/aktualizr/pull/1180)


## [2019.2] - 2019-02-21

### Added

- A new uptane configuration parameter `force_install_completion` that triggers a system reboot at the end of the installation process for update mechanisms that need one to complete (e.g. OSTree package manager)
- Support for delegations: [PR #1074](https://github.com/advancedtelematic/aktualizr/pull/1074) and [PR #1089](https://github.com/advancedtelematic/aktualizr/pull/1089)
- Backward migrations of the SQL storage is now supported. It should allow rollbacking updates up to versions containing the feature: [PR](https://github.com/advancedtelematic/aktualizr/pull/1072)

### Changed

- Image files are now stored on the filesystem instead of SQL. This was necessitated by blob size limits in SQLite. [PR](https://github.com/advancedtelematic/aktualizr/pull/1091)
- The Pause and Resume can now be called at any time and will also pause the internal event queue. API calls during the pause period will be queued up and resumed in order at the end. [PR](https://github.com/advancedtelematic/aktualizr/pull/1075)
- Boost libraries are now linked dynamically (as with all other dependencies): [PR](https://github.com/advancedtelematic/aktualizr/pull/1067)


## [2019.1] - 2019-01-10

### Changed

- [Most API calls refactored to return immediately with a future](src/libaktualizr/primary/aktualizr.h)
- With an OStree primary, an installation is now considered successful when the device reboots with the new file system version. Before that, the installation is still considered in progress.
- [Running modes in libaktualizr have been replaced by simpler logic in the aktualizr wrapper](src/aktualizr_primary/main.cc): [PR](https://github.com/advancedtelematic/aktualizr/pull/1039)
- Tests now use ed25519 as the default key type: [PR](https://github.com/advancedtelematic/aktualizr/pull/1038)
- Improved performance of garage-deploy: [PR](https://github.com/advancedtelematic/aktualizr/pull/1020)

### Added

- Auto retry for more robust download: [PR](https://github.com/advancedtelematic/aktualizr/pull/1001)
- Expanded functionality of aktualizr-repo: [PR #1028](https://github.com/advancedtelematic/aktualizr/pull/1028) and [PR #1035](https://github.com/advancedtelematic/aktualizr/pull/1035)
- Option to run garage-push and garage-check to walk the full repository tree: [PR](https://github.com/advancedtelematic/aktualizr/pull/1020)
- Ability to pause and resume OSTree update downloads: [PR](https://github.com/advancedtelematic/aktualizr/pull/1007)

### Removed

- Downloads are no longer done in parallel, as this substantially impacted the download speed: [PR](https://github.com/advancedtelematic/aktualizr/pull/1031)

### Fixed

- Correctly download targets with characters disallowed in URI in their name: [PR](https://github.com/advancedtelematic/aktualizr/pull/996)


## [2018.13] - 2018-11-05

### Added

- [Ability to pause and resume binary update downloads](src/libaktualizr/primary/aktualizr.h)
- Expose download binary targets in API

### Changed

- Secondaries configuration files must now lie in a common directory and specified in command line arguments or in static configuration: [documentation](docs/configuration.adoc#uptane)
- API has been upgraded: FetchMeta has been merged with CheckUpdates and most functions now have meaningful return values.

### Removed

- implicit_writer has been removed as it was no longer being used.

### Fixed

- Now trim whitespaces in some of our configuration and provisioning files ([from meta-updater #420](https://github.com/advancedtelematic/meta-updater/issues/420))


## [2018.12] - 2018-10-10

### Changed

- Various updates in API
- `sota_implicit_prov` is deprecated
- All the imported data should be under /var/sota/import

### Fixed

- HSM provisioning should not import certificate and private key, they belong to HSM, not to storage
- Make cert provider respect path to import directory


## [2018.11] - 2018-09-05

### Fixed

- Really remove the local tuf repo before and after garage-sign.


## [2018.10] - 2018-09-04

### Added

- garage-deploy and aktualizr releases for Ubuntu 18.04

### Fixed

- Prevent re-use of existing tuf repos


## [2018.9] - 2018-08-30

### Fixed

- Fixes to garage-deploy to improve reliability and logging


## [2018.8] - 2018-08-16

### Fixed

- Bug with path concatenation in garage-deploy


## [2018.7] - 2018-05-31

### Changed

- garage-deploy package is now built against Ubuntu 16.04


## [2018.6] - 2018-05-28

### Fixed

- Expiration in garage-sign


## [2018.5] - 2018-02-26

## [2018.4] - 2018-02-23

## [2018.3] - 2018-02-16

## [2018.2] - 2018-02-16

## [2018.1] - 2018-02-05
