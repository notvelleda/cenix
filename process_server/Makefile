PROJECT_ROOT = ..

CFLAGS += -fPIC -fPIE -msep-data -Os
LDFLAGS += -Wl,-elf2flt -Wl,-move-rodata
BINARY = process_server

.include <$(PROJECT_ROOT)/common-vars.mk>

INITRD_DIR=../
INITRD_FILE=initrd.tar
INITRD_OBJECT=$(CWD)/src/initrd.o

OBJECTS += $(INITRD_OBJECT)

.include <$(PROJECT_ROOT)/binary.mk>

$(INITRD_OBJECT): $(INITRD_DIR)/$(INITRD_FILE)
	cd $(INITRD_DIR) && $(CROSS)objcopy -I binary -O $(BINARY_FORMAT) --rename-section .data=.rodata,alloc,load,readonly,data,contents $(INITRD_FILE) $(INITRD_OBJECT)
