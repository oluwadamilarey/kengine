BUILD_DIR := bin
OBJ_DIR := obj

ASSEMBLY := engine
EXTENSION := .dylib
COMPILER_FLAGS := -g -fdeclspec -fvisibility=hidden -ObjC
ifndef VULKAN_SDK
$(error VULKAN_SDK is not set. Export it before building (e.g. export VULKAN_SDK=$$HOME/VulkanSDK/1.4.350.0/macOS))
endif
# Accept either VULKAN_SDK=<version>/macOS or VULKAN_SDK=<version>.
VULKAN_INCLUDE_DIR := $(if $(wildcard $(VULKAN_SDK)/include),$(VULKAN_SDK)/include,$(VULKAN_SDK)/macOS/include)
VULKAN_LIB_DIR := $(if $(wildcard $(VULKAN_SDK)/lib),$(VULKAN_SDK)/lib,$(VULKAN_SDK)/macOS/lib)
# $(VULKAN_SDK)/include MUST precede all system paths so vulkan_metal.h
# from MoltenVK is found instead of a system Vulkan install that lacks it.
INCLUDE_FLAGS := -Iengine/src -I$(VULKAN_INCLUDE_DIR)
LINKER_FLAGS := -g -shared \
	-install_name @rpath/lib$(ASSEMBLY)$(EXTENSION) \
	-lvulkan \
	-L$(VULKAN_LIB_DIR) \
	-rpath $(VULKAN_LIB_DIR) \
	-framework Cocoa \
	-framework QuartzCore \
	-framework Metal
DEFINES := -D_DEBUG -DKEXPORT -DKPLATFORM_APPLE

C_FILES    := $(shell find $(ASSEMBLY) -name '*.c')
M_FILES    := $(shell find $(ASSEMBLY) -name '*.m')
DIRECTORIES := $(shell find $(ASSEMBLY) -type d)
OBJ_FILES  := $(C_FILES:%=$(OBJ_DIR)/%.o) $(M_FILES:%=$(OBJ_DIR)/%.o)

all: scaffold compile link

.PHONY: scaffold
scaffold:
	@echo Scaffolding folder structure...
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
	@echo Done.

.PHONY: compile
compile:
	@echo Compiling $(ASSEMBLY)...

.PHONY: link
link: scaffold $(OBJ_FILES)
	@echo Linking $(ASSEMBLY)...
	@clang $(OBJ_FILES) -o $(BUILD_DIR)/lib$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
	@echo Linked: $(BUILD_DIR)/lib$(ASSEMBLY)$(EXTENSION)

.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/lib$(ASSEMBLY)$(EXTENSION)
	rm -rf $(OBJ_DIR)/$(ASSEMBLY)

$(OBJ_DIR)/%.c.o: %.c
	@echo   $<...
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

$(OBJ_DIR)/%.m.o: %.m
	@echo   $<...
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)