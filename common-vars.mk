CROSS ?= m68k-unknown-elf-
CC ?= $(CROSS)cc

ARCH_68000 != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "68000" || echo ""
ARCH = $(ARCH_68000)

DEBUG_FLAG != [ "$(DEBUG)" = y ] && echo "-DDEBUG" || echo ""

CFLAGS += -fomit-frame-pointer -Isrc -Iinclude -Iprintf -nolibc -nostartfiles -fno-builtin -ffreestanding -fno-stack-protector -static -Wall -DPLATFORM="$(PLATFORM)" $(DEBUG_FLAG) -DARCH_$(ARCH)
LDFLAGS +=

ARCH_PATH = src/arch/$(ARCH)
PLATFORM_PATH = src/platform/$(PLATFORM)

# architecture-specific cc flags
68000_CFLAGS != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "-m68000" || echo ""
CFLAGS += $(68000_CFLAGS)

ARCH_SOURCES != find $(ARCH_PATH) -name "*.c" -o -name "*.S"
PLATFORM_SOURCES != find $(PLATFORM_PATH) -name "*.c" -o -name "*.S"
COMMON_SOURCES != find src -maxdepth 1 -name "*.c"
SOURCE_FILES = $(COMMON_SOURCES) $(PLATFORM_SOURCES) $(ARCH_SOURCES) $(ADDITIONAL_SOURCES)

C_OBJECTS = $(SOURCE_FILES:.c=.o)
OBJECTS = $(C_OBJECTS:.S=.o)
