CROSS ?= m68k-unknown-elf-
CC ?= $(CROSS)cc
LD ?= $(CROSS)ld

ARCH_68000 != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "68000" || echo ""
ARCH = $(ARCH_68000)

DEBUG_FLAG != [ "$(DEBUG)" = y ] && echo "-DDEBUG" || echo ""

CFLAGS += -O2 -Isrc -Iinclude -Iprintf -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -static -Wall -DPLATFORM="$(PLATFORM)" $(DEBUG_FLAG) -DARCH_$(ARCH)

ARCH_PATH = src/arch/$(ARCH)
PLATFORM_PATH = src/platform/$(PLATFORM)

# architecture-specific cc flags
68000_CFLAGS != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "-m68000 -DBITS=32" || echo ""
CFLAGS += $(68000_CFLAGS)

# platform-specific ld flags
# these are used for things like specifying a linker script to be used if something other than the default is required
MAC_LDFLAGS != [ "$(PLATFORM)" = mac-68000 ] && echo "-T$(PLATFORM_PATH)/kernel.ld" || echo ""
LDFLAGS += $(MAC_LDFLAGS)

ARCH_SOURCES != find $(ARCH_PATH) -name "*.c" -o -name "*.S"
PLATFORM_SOURCES != find $(PLATFORM_PATH) -name "*.c" -o -name "*.S"
COMMON_SOURCES != find src -maxdepth 1 -name "*.c"

PRINTF_SOURCES != [ "$(DEBUG)" = y ] && echo "printf/printf.c" || echo ""

SOURCE_FILES = $(COMMON_SOURCES) $(PLATFORM_SOURCES) $(ARCH_SOURCES) $(PRINTF_SOURCES)
C_OBJECTS = $(SOURCE_FILES:.c=.o)
OBJECTS = $(C_OBJECTS:.S=.o)

BINARY = kernel

all: $(BINARY)

.PHONY: clean

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-find . -name "*.o" | xargs -n1 rm
	-rm $(BINARY)
	-rm .depend
