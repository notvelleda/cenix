CFLAGS += -fPIC -fPIE -msep-data -Os
LDFLAGS += -Wl,-elf2flt -Wl,-move-rodata -L../common
LIBRARIES += -lcommon
BINARY = vfs_server

.include "$(PROJECT_ROOT)/makefiles/common-vars.mk"
.include "$(PROJECT_ROOT)/makefiles/binary.mk"
