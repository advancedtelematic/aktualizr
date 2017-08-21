#!/bin/bash

echo $PWD
mkdir "$2"
cd "$2"
cmake $3 $1
make -j5 aktualizr
