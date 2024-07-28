ADDITIONAL_SOURCES != [ "$(DEBUG)" = y ] && echo "printf/printf.c" || echo ""

BINARY = kernel

.include <../common-vars.mk>

# platform-specific ld flags
# these are used for things like specifying a linker script to be used if something other than the default is required
MAC_LDFLAGS != [ "$(PLATFORM)" = mac-68000 ] && echo "-T$(PLATFORM_PATH)/kernel.ld" || echo ""
LDFLAGS += $(MAC_LDFLAGS)

CWD != pwd

INIT_DIR=../init
INIT_FILE=init
INIT_OBJECT=$(CWD)/src/init.o

OBJECTS += $(INIT_OBJECT)

.include <../common-targets.mk>

# the binary format for the object that's converted from the init binary
BINARY_68000 != [ "$(ARCH)" = 68000 ] && echo "elf32-m68k" || echo ""
BINARY_FORMAT = $(BINARY_68000)

$(INIT_OBJECT): $(INIT_DIR)/$(INIT_FILE)
	cd $(INIT_DIR) && $(CROSS)objcopy -I binary -O $(BINARY_FORMAT) $(INIT_FILE) $(INIT_OBJECT)
