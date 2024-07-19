LDFLAGS += -melf_i386 -Tsrc/arch/i386/kernel.ld
ZIGFLAGS += -target x86-freestanding -mcpu i386 -fno-stack-check -O ReleaseSafe
ASFLAGS += -32 -march=i386

OBJECTS = src/main.o src/arch/i386/init.o src/arch/i386/isr.o
BINARY = kernel

$(BINARY): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

.PHONY: src/main.o
src/main.o: src/main.zig
	cd src && zig build-obj main.zig $(ZIGFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY) src/main.o.o
