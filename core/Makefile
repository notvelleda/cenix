kernel: process_server # kernel requires process server to be built so it can be embedded inside

SUBDIRECTORIES_FILTER = -not \( -path printf/Makefile -prune \)
BUILD_DIRECTORY = $(PROJECT_ROOT)/build

INITRD_PATH = $(BUILD_DIRECTORY)/initrd.jax
INITRD_DIR_PATH = $(BUILD_DIRECTORY)/initrd

process_server: $(INITRD_PATH) # process server requires initrd to be built so it can be embedded inside

SBIN_CONTENTS = vfs_server service_manager debug_console initrd_fs

$(INITRD_PATH): $(SBIN_CONTENTS)
	@# create root directory tree
	mkdir -p $(INITRD_DIR_PATH)/sbin
	mkdir -p $(INITRD_DIR_PATH)/proc
	mkdir -p $(INITRD_DIR_PATH)/dev
	@# populate /sbin
.for file in $(SBIN_CONTENTS)
	cp $(BUILD_DIRECTORY)/$(file)/$(file) $(INITRD_DIR_PATH)/sbin/
.endfor
	@# create the archive file
	cd $(INITRD_DIR_PATH) && $(PWD)/$(BUILD_DIRECTORY)/jax/jax -cf $(PWD)/$@ *

.include <$(PROJECT_ROOT)/makefiles/build-subdirectories.mk>
