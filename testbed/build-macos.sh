#!/bin/bash
# Build script for testbed - macOS
set -e

mkdir -p ../bin

# Get a list of all the .c files.
cFilenames=$(find . -type f -name "*.c")

assembly="testbed"
compilerFlags="-g -mmacosx-version-min=10.15"

# Include paths - reference the engine
includeFlags="-Isrc -I../engine/src -I$VULKAN_SDK/include $(pkg-config --cflags glfw3)"

# Link against the engine library and GLFW
linkerFlags="-L../bin -lengine -rpath ../bin -L$VULKAN_SDK/lib $(pkg-config --libs glfw3) -framework Cocoa -framework IOKit -framework CoreFoundation -framework QuartzCore"

defines="-D_DEBUG -DVK_USE_PLATFORM_METAL_EXT"

echo "Building $assembly..."
clang $cFilenames $compilerFlags -o ../bin/$assembly $defines $includeFlags $linkerFlags

echo "Testbed build complete."