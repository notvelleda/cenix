CROSS ?= m68k-unknown-elf-
CC = $(CROSS)cc
AR = $(CROSS)ar

CWD != pwd

ARCH_68000 != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "68000" || echo ""
ARCH = $(ARCH_68000)

DEBUG_FLAG != [ "$(DEBUG)" = y ] && echo "-DDEBUG" || echo ""

CFLAGS += -Isrc -Iinclude -I$(PROJECT_ROOT)/core/include -I$(PROJECT_ROOT)/core/common -I$(PROJECT_ROOT)/core/printf
CFLAGS += -fomit-frame-pointer -nolibc -nostartfiles -fno-builtin -ffreestanding -fno-stack-protector -static -Wall
CFLAGS += -DPRINTF_DISABLE_SUPPORT_FLOAT -DPLATFORM="$(PLATFORM)" $(DEBUG_FLAG) -DARCH_$(ARCH)

ARCH_PATH = src/arch/$(ARCH)
PLATFORM_PATH = src/platform/$(PLATFORM)

# architecture-specific cc flags
68000_CFLAGS != [ -n "`echo $(PLATFORM) | grep -e '.*-68000'`" ] && echo "-m68000" || echo ""
CFLAGS += $(68000_CFLAGS)

# the binary format for the object that's converted from the init binary
BINARY_68000 != [ "$(ARCH)" = 68000 ] && echo "elf32-m68k" || echo ""
BINARY_FORMAT = $(BINARY_68000)

ARCH_SOURCES != find $(ARCH_PATH) -name "*.c" -o -name "*.S" 2>/dev/null || true
PLATFORM_SOURCES != find $(PLATFORM_PATH) -name "*.c" -o -name "*.S" 2>/dev/null || true
COMMON_SOURCES != find src -maxdepth 1 -name "*.c" 2>/dev/null || true
SOURCE_FILES = $(COMMON_SOURCES) $(PLATFORM_SOURCES) $(ARCH_SOURCES) $(ADDITIONAL_SOURCES)

DEBUG_OBJECTS_COND != [ "$(DEBUG)" = y ] && echo $(DEBUG_OBJECTS) || echo ""

C_OBJECTS = $(SOURCE_FILES:.c=.o) $(DEBUG_OBJECTS_COND)
OBJECTS += $(C_OBJECTS:.S=.o)
