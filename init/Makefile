CFLAGS += -fPIC -fPIE -msep-data -Os
LDFLAGS += -Wl,-elf2flt -Wl,-move-rodata
BINARY = init

.include <../common-vars.mk>

INITRD_DIR=../
INITRD_FILE=initrd.tar
INITRD_OBJECT=$(CWD)/src/initrd.o

OBJECTS += $(INITRD_OBJECT)

.include <../binary.mk>

$(INITRD_OBJECT): $(INITRD_DIR)/$(INITRD_FILE)
	cd $(INITRD_DIR) && $(CROSS)objcopy -I binary -O $(BINARY_FORMAT) --rename-section .data=.rodata,alloc,load,readonly,data,contents $(INITRD_FILE) $(INITRD_OBJECT)
