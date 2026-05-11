#!/bin/bash
# Build script for rebuilding everything on macOS (MoltenVK/Apple Silicon)
set -e

echo "Building everything (macOS)..."

make -f Makefile.engine.macos.mak all
if [ $? -ne 0 ]; then
    echo "Error building engine: $?" && exit 1
fi

make -f Makefile.testbed.macos.mak all
if [ $? -ne 0 ]; then
    echo "Error building testbed: $?" && exit 1
fi

echo "All assemblies built successfully."