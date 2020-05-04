#! /bin/bash
set -eEo pipefail

if [ -n "$DOCKER_APP_FAIL" ] ; then
  echo "FAILING the fake docker app command"
  exit 1
fi

if [ "$1" = "app" ] ; then
  HERE=$(dirname $0)
  if [ "$2" = "pull" ] ; then
    echo PULL CALLED $* > $HERE/docker-app-pull
    exit 0
  fi
  if [ "$2" = "image" ] ; then
    if [ "$3" == "render" ] ; then
      echo "FAKE CONTENT FOR IMAGE RENDER" > $5
      echo $6 >> $5
      exit 0
    fi
  fi
fi

if [ "$1" = "render" ] ; then
  if [ ! -f app1.dockerapp ] ; then
    echo "Missing docker app file!"
    exit 1
  fi
  if [ "$3" != "-o" ] ; then
    echo "Unexpected arguments to render: $*"
    exit 1
  fi
  echo "DOCKER-APP RENDER OUTPUT" > $4
  cat app1.dockerapp >> $4
  exit 0
fi
if [ "$1" = "pull" ] ; then
  echo "DOCKER-COMPOSE PULL"
  if [ ! -f docker-compose.yml ] ; then
    echo "Missing docker-compose file!"
    exit 1
  fi
  exit 0
fi
if [ "$1" = "up" ] ; then
  echo "DOCKER-COMPOSE UP"
  if [ ! -f docker-compose.yml ] ; then
    echo "Missing docker-compose file!"
    exit 1
  fi
  # the content of dockerapp includes the sha of the target, so this should
  # be present in the docker-compose.yml it creates.
  if ! grep primary docker-compose.yml ; then
    echo "Could not find expected content in docker-compose.yml"
    cat docker-compose.yml
    exit 1
  fi
  exit 0
fi
if [ "$1" = "down" ] ; then
  echo "Fake downing the docker-app in $(pwd)"
  touch ../docker-compose-down-called
  exit 0
fi
if [ "$1" = "image" ] ; then
  if [ "$2" = "prune" ] ; then
    exit 0
  fi
fi
echo "Unknown command: $*"
exit 1
