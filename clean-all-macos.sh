#!/bin/bash
# Clean script for all assemblies on macOS
set -e

echo "Cleaning everything (macOS)..."

make -f Makefile.engine.macos.mak clean
if [ $? -ne 0 ]; then
    echo "Error cleaning engine: $?" && exit 1
fi

make -f Makefile.testbed.macos.mak clean
if [ $? -ne 0 ]; then
    echo "Error cleaning testbed: $?" && exit 1
fi

echo "All assemblies cleaned successfully."