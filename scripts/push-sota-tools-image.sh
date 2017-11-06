#! /bin/bash
set -euo pipefail

DOCKERFILE=src/sota_tools/Dockerfile
docker build -t advancedtelematic/sota-tools -f ${DOCKERFILE} .
docker login -u="$DOCKER_USERNAME" -p="$DOCKER_PASSWORD"
docker push advancedtelematic/sota-tools
