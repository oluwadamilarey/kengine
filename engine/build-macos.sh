#!/bin/bash
# Build script for Kohi engine - macOS
set -e

mkdir -p ../bin

cFilenames=$(find . -type f -name "*.c")
assembly="engine"
compilerFlags="-g -shared -fPIC -mmacosx-version-min=10.15"

GLFW_PREFIX=$(brew --prefix glfw)
includeFlags="-Isrc -I$VULKAN_SDK/include -I$VULKAN_SDK/include/MoltenVK -I$GLFW_PREFIX/include"

# Add proper rpath for Vulkan SDK
linkerFlags="-lvulkan -L$VULKAN_SDK/lib -L$GLFW_PREFIX/lib -lglfw -framework Cocoa -framework IOKit -framework CoreFoundation -framework QuartzCore -framework Metal -Wl,-rpath,$VULKAN_SDK/lib"

defines="-D_DEBUG -DKEXPORT -DVK_USE_PLATFORM_METAL_EXT"

echo "Building $assembly..."
clang $cFilenames $compilerFlags -o ../bin/lib$assembly.dylib $defines $includeFlags $linkerFlags

# Set proper install name and rpath
install_name_tool -id "@rpath/lib$assembly.dylib" ../bin/lib$assembly.dylib

echo "Engine build complete."