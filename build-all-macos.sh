#!/bin/bash
# Build script for rebuilding everything on macOS (MoltenVK/Apple Silicon)
set -e
if [ -z "$VULKAN_SDK" ]; then
    echo "Error: VULKAN_SDK is not set."
    echo "Example: export VULKAN_SDK=\"$HOME/VulkanSDK/1.4.350.0/macOS\""
    exit 1
fi

# Accept either VULKAN_SDK=<version>/macOS or VULKAN_SDK=<version>.
if [ -d "$VULKAN_SDK/include" ] && [ -d "$VULKAN_SDK/lib" ]; then
    RESOLVED_VULKAN_SDK="$VULKAN_SDK"
elif [ -d "$VULKAN_SDK/macOS/include" ] && [ -d "$VULKAN_SDK/macOS/lib" ]; then
    RESOLVED_VULKAN_SDK="$VULKAN_SDK/macOS"
else
    echo "Error: VULKAN_SDK does not contain expected include/lib directories."
    echo "Checked: $VULKAN_SDK and $VULKAN_SDK/macOS"
    exit 1
fi

export VULKAN_SDK="$RESOLVED_VULKAN_SDK"

echo "Building everything (macOS)..."
echo "Using VULKAN_SDK=$VULKAN_SDK"

make -f Makefile.engine.macos.mak all
if [ $? -ne 0 ]; then
    echo "Error building engine: $?" && exit 1
fi

make -f Makefile.testbed.macos.mak all
if [ $? -ne 0 ]; then
    echo "Error building testbed: $?" && exit 1
fi

echo "All assemblies built successfully."