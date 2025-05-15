kernel: process_server # kernel requires process server to be built so it can be embedded inside

SUBDIRECTORIES_FILTER = -not \( -path printf/Makefile -prune \)
BUILD_DIRECTORY = $(PROJECT_ROOT)/build

INITRD_PATH = $(BUILD_DIRECTORY)/initrd.jax
INITRD_DIR_PATH = $(BUILD_DIRECTORY)/initrd

process_server: common $(INITRD_PATH) # process server requires common library and requires initrd to be built so it can be embedded inside
# this should also hopefully force common to be built before everything that depends on it without having to manually write out all of these dependency relationships

CORE_DIR_CONTENTS = vfs_server service_manager debug_console initrd_fs

$(INITRD_PATH): $(CORE_DIR_CONTENTS)
	@# create root directory tree
	mkdir -p $(INITRD_DIR_PATH)/lib/core
	mkdir -p $(INITRD_DIR_PATH)/proc
	mkdir -p $(INITRD_DIR_PATH)/dev
	@# populate /lib/core
.for file in $(CORE_DIR_CONTENTS)
	cp $(BUILD_DIRECTORY)/$(file)/$(file) $(INITRD_DIR_PATH)/lib/core/
.endfor
	@# create the archive file
	cd $(INITRD_DIR_PATH) && $(BUILD_DIRECTORY)/jax/jax -cf $@ *

.include "$(PROJECT_ROOT)/makefiles/build-subdirectories.mk"
