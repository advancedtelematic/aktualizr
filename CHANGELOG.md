# Changelog

This file summarizes notable changes introduced in aktualizr version. It roughly follows the guidelines from [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

Our versioning scheme is `YEAR.N` where `N` is incremented whenever a new release is deemed necessary. Thus it does not exactly map to months of the year.

## [??? (unreleased)]

### Changed

- [Most API calls refactored to return immediately with a future](src/libaktualizr/primary/aktualizr.h)
- With an OStree primary, an installation is now considered successful when the device reboots with the new file system version. Before that, the installation is still considered in progress.

### Added

- Auto retry for more robust download: [PR](https://github.com/advancedtelematic/aktualizr/pull/1001)

### Fixed

- Correctly download targets with characters disallowed in URI in their name: [PR](https://github.com/advancedtelematic/aktualizr/pull/996)

## [2018.13] - 2018-11-05

### Added

- [Ability to pause and resume update downloads](src/libaktualizr/primary/aktualizr.h)
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
