PROJECT_ROOT = ../..

CFLAGS += -fPIC -fPIE -msep-data -Os
LDFLAGS += -Wl,-elf2flt -Wl,-move-rodata
BINARY = vfs_server

.include <$(PROJECT_ROOT)/makefiles/common-vars.mk>
.include <$(PROJECT_ROOT)/makefiles/binary.mk>
