#!/bin/bash

echo $PWD
# Protect against mistakes by not just blindly deleting the second input
# argument.
rm -Rf "${2}_prev"
mv $2 "${2}_prev"
mkdir -p "$2"
cd "$2"
cmake $3 $1
make -j5 aktualizr
