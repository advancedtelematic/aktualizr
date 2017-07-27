# OSTree sysroot generators

Scripts for generating minimal OSTree sysroots primarily for [load tests](https://github.com/advancedtelematic/ota-load-tests-aktualizr).

## Usage

```
./makephysical.sh /some/path/ # Contents of /some/path will be deleted
```

makedeployed.sh can also be used as a standalone script for creating deployable sysroots.

```
./makedeployed.sh /path/to/ostree/repo branchname hardware_is
```
