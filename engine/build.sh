#!/bin/bash
# Build script for engine
set echo on

mkdir -p ../bin

# Get a list of all the .c files.
cFilenames=$(find . -type f -name "*.c")

# echo "Files:" $cFilenames

assembly="engine"
compilerFlags="-g -shared -fdeclspec -fPIC"
# -fms-extensions 
# -Wall -Werror
includeFlags="-Isrc -I$VULKAN_SDK/include"
linkerFlags="-lvulkan -lxcb -lX11 -lX11-xcb -lxkbcommon -L$VULKAN_SDK/lib -L/usr/X11R6/lib"
defines="-D_DEBUG -DKEXPORT"

echo "Building $assembly..."
clang $cFilenames $compilerFlags -o ../bin/lib$assembly.so $defines $includeFlags $


# #!/bin/bash
# # Build script for Vulkan engine - macOS version
# set -e  # Exit on error (more robust than 'set echo on')

# mkdir -p ../bin

# # Get a list of all the .c files.
# cFilenames=$(find . -type f -name "*.c")

# # echo "Files:" $cFilenames

# assembly="engine"
# compilerFlags="-g -shared -fPIC -mmacosx-version-min=10.15"
# # Additional macOS-specific flags you might need:
# # -arch x86_64 -arch arm64  # for universal binary
# # -Wall -Werror
# includeFlags="-Isrc -I$VULKAN_SDK/include -I$VULKAN_SDK/include/MoltenVK"
# linkerFlags="-lvulkan -L$VULKAN_SDK/lib -framework Cocoa -framework IOKit -framework CoreFoundation -framework QuartzCore -framework Metal"
# defines="-D_DEBUG -DKEXPORT -DVK_USE_PLATFORM_MACOS_MVK -DVK_USE_PLATFORM_METAL_EXT"

# # Optional: Add MoltenVK specific linking if needed
# # linkerFlags="$linkerFlags -lMoltenVK"

# echo "Building $assembly..."
# clang $cFilenames $compilerFlags -o ../bin/lib$assembly.dylib $defines $includeFlags $linkerFlags

# # Set proper install name for the dylib (important for runtime linking)
# install_name_tool -id "@rpath/lib$assembly.dylib" ../bin/lib$assembly.dylib

# echo "Build complete. Output: ../bin/lib$assembly.dylib"