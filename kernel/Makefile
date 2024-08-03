PROJECT_ROOT = ..

CFLAGS += -O2
BINARY = kernel

.include <$(PROJECT_ROOT)/common-vars.mk>

# platform-specific ld flags
# these are used for things like specifying a linker script to be used if something other than the default is required
MAC_LDFLAGS != [ "$(PLATFORM)" = mac-68000 ] && echo "-T$(PLATFORM_PATH)/kernel.ld" || echo ""
LDFLAGS += $(MAC_LDFLAGS)

PROCESS_SERVER_DIR=../process_server
PROCESS_SERVER_FILE=process_server
PROCESS_SERVER_OBJECT=$(CWD)/src/process_server.o

OBJECTS += $(PROCESS_SERVER_OBJECT)

.include <$(PROJECT_ROOT)/binary.mk>

OBJCOPY_FLAGS = -I binary -O $(BINARY_FORMAT) --rename-section .data=.rodata,alloc,load,readonly,data,contents

$(PROCESS_SERVER_OBJECT): $(PROCESS_SERVER_DIR)/$(PROCESS_SERVER_FILE)
	cd $(PROCESS_SERVER_DIR) && $(CROSS)objcopy $(OBJCOPY_FLAGS) $(PROCESS_SERVER_FILE) $(PROCESS_SERVER_OBJECT)
