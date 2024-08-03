PROJECT_ROOT = ..

CFLAGS += -fPIC -fPIE -msep-data -Os
LDFLAGS += -Wl,-elf2flt -Wl,-move-rodata
BINARY = initrd_tar_fs

.include <$(PROJECT_ROOT)/common-vars.mk>
.include <$(PROJECT_ROOT)/binary.mk>
