#!/usr/bin/env bash

set -euo pipefail

# returns the tcp port a server application listens to
# dependency: netstat (net-tools)

pid=$1

port=
try=0
while [[ -z $port  && $try -le 100 ]]; do
    port=$(netstat -lpan 2> /dev/null | awk -F '[ :]+' "/$pid\\/python3/ {print \$5}")
    sleep 0.2
    try=$((try+1))
done
echo "$port"
