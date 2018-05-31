#! /bin/bash

set -exuo pipefail

dpkg -i /persistent/garage_deploy.deb
garage-deploy --version
garage-sign --help
