BUILD_DIR := bin
OBJ_DIR := obj

ASSEMBLY := testbed
EXTENSION :=
COMPILER_FLAGS := -g -MD \
                  -Wvla \
                  -Werror=vla \
                  -Wvla-extension \
                  -Werror=vla-extension \
                  -fdeclspec \
                  -fPIC
INCLUDE_FLAGS := -Iengine/src -Itestbed/src
LINKER_FLAGS := -g \
	-L./$(BUILD_DIR) \
	-lengine \
	-Wl,-rpath,@loader_path \
	-Wl,-rpath,$(BUILD_DIR)
DEFINES := -D_DEBUG -DKIMPORT

SRC_FILES := $(shell find $(ASSEMBLY) -name '*.c')
DIRECTORIES := $(shell find $(ASSEMBLY) -type d)
OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)

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
	@clang $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
	@echo Linked: $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)

.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
	rm -rf $(OBJ_DIR)/$(ASSEMBLY)

$(OBJ_DIR)/%.c.o: %.c
	@echo   $<...
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)