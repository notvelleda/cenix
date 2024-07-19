CROSS ?= m68k-unknown-elf-
CC ?= $(CROSS)cc
LD ?= $(CROSS)ld

CFLAGS += -O2 -Isrc -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -static -Wall -DPLATFORM=$(PLATFORM)

PLATFORM_PATH = src/platform/$(PLATFORM)

# architecture-specific cc flags
68000_CFLAGS != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "-m68000 -DBITS=32" || echo ""
CFLAGS += $(68000_CFLAGS)

# platform-specific ld flags
# these are used for things like specifying a linker script to be used if something other than the default is required
MAC_LDFLAGS != [ "$(PLATFORM)" = mac-68000 ] && echo "-T$(PLATFORM_PATH)/kernel.ld" || echo ""
LDFLAGS += $(MAC_LDFLAGS)

PLATFORM_SOURCES != find $(PLATFORM_PATH) -name "*.c" -o -name "*.S"
COMMON_SOURCES != find src -maxdepth 1 -name "*.c"

# architecture-specific source files
68000_SOURCES != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && find src/arch/68000 -name "*.c" -o -name "*.S" || echo ""
PLATFORM_SOURCES += $(68000_SOURCES)

SOURCE_FILES = $(COMMON_SOURCES) $(PLATFORM_SOURCES)
C_OBJECTS = $(SOURCE_FILES:.c=.o)
OBJECTS = $(C_OBJECTS:.S=.o)

BINARY = kernel

all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-find . -name "*.o" | xargs -n1 rm
	-rm $(BINARY)
	-rm .depend
