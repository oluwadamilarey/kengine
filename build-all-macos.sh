#!/bin/bash
# Build all script for Kohi engine - macOS
set -e

echo "Building Kohi Engine for macOS..."

pushd engine
source build-macos.sh
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]; then
    echo "Error: Engine build failed" && exit $ERRORLEVEL
fi
popd

pushd testbed  
source build-macos.sh
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]; then
    echo "Error: Testbed build failed" && exit $ERRORLEVEL
fi
popd

echo "All assemblies built successfully for macOS."