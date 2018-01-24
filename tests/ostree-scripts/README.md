# OSTree sysroot generators

Scripts for generating minimal OSTree sysroots for testing purposes.

## Usage

```
./makephysical.sh /some/path/ # Contents of /some/path will be deleted
```

makedeployed.sh can also be used as a standalone script for creating deployable sysroots.

```
./makedeployed.sh /path/to/ostree/repo branchname hardware_is
```
