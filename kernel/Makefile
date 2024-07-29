CFLAGS += -O2
BINARY = kernel

.include <../common-vars.mk>

# platform-specific ld flags
# these are used for things like specifying a linker script to be used if something other than the default is required
MAC_LDFLAGS != [ "$(PLATFORM)" = mac-68000 ] && echo "-T$(PLATFORM_PATH)/kernel.ld" || echo ""
LDFLAGS += $(MAC_LDFLAGS)

INIT_DIR=../init
INIT_FILE=init
INIT_OBJECT=$(CWD)/src/init.o

OBJECTS += $(INIT_OBJECT)

.include <../binary.mk>

$(INIT_OBJECT): $(INIT_DIR)/$(INIT_FILE)
	cd $(INIT_DIR) && $(CROSS)objcopy -I binary -O $(BINARY_FORMAT) --rename-section .data=.rodata,alloc,load,readonly,data,contents $(INIT_FILE) $(INIT_OBJECT)
