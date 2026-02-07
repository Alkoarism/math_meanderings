#!/bin/bash
set -e

PROG_NAME=core_consolidation_linux_build

make

echo "Launching: $PROG_NAME"
./bin/$PROG_NAME
