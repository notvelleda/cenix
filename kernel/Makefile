LDFLAGS += -melf_i386 -Tkernel.ld
ZIGFLAGS += -target x86-freestanding -fno-stack-check -O ReleaseSmall
ASFLAGS += -32 -march=i386

OBJECTS = src/main.o src/init.o
BINARY = kernel

$(BINARY): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

src/main.o: src/main.zig
	cd src && zig build-obj main.zig $(ZIGFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY) src/main.o.o
