#!/usr/bin/env bash
set -e

docker build -f $1 . -t $2
docker run --rm $2
