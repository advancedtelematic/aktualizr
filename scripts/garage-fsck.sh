#!/bin/bash

CREDENTIALS="${1}"
OSTREE_REF="${2}"
GITREPO_ROOT="$(readlink -f "$(dirname "$0")/..")"
GARAGE_CHECK="${3:-${GITREPO_ROOT}/build/src/sota_tools/garage-check}"
LOCAL_REPO="${4:-temp-repo}"

"${GARAGE_CHECK}" -j "${CREDENTIALS}" -w --ref "${OSTREE_REF}" -t "${LOCAL_REPO}"

cat << EOF >> "${LOCAL_REPO}/config"
[core]
repo_version=1
mode=archive-z2
EOF

# This directory is required despite that it appears to be unused.
mkdir -p "${LOCAL_REPO}/refs/remotes"
mkdir -p "${LOCAL_REPO}/refs/heads"
echo -n "${OSTREE_REF}" > "${LOCAL_REPO}/refs/heads/master"

ostree fsck --repo "${LOCAL_REPO}"
