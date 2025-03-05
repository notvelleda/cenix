.PHONY: test

all: $(BINARY) test

DEBUG_FLAG != [ "$(DEBUG)" = y ] && echo "-DDEBUG" || echo ""

CFLAGS += -O2 -I$(CWD) -I$(CWD)/$(PROJECT_ROOT)/core/include -I$(CWD)/$(PROJECT_ROOT)/core/common -I$(CWD)/$(PROJECT_ROOT)/test/include -DUNDER_TEST $(DEBUG_FLAG) -D_start=entry_point
LDFLAGS += -L$(CWD)/$(PROJECT_ROOT)/build/userland_low_level
LIBFLAGS = -l$(TEST_HARNESS)
SOURCE_FILES != find . -name "*.c" 2>/dev/null
OBJECTS = $(SOURCE_FILES:.c=.o)
OBJ_DIR_PREFIX = test/

.include "$(PROJECT_ROOT)/makefiles/binary.mk"

test: $(BINARY)
	$(CWD)/$(MAKEOBJDIR)/$(BINARY)
