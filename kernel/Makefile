LDFLAGS += -melf_i386 -Tkernel.ld
ZIGFLAGS += -target i386-freestanding -fno-stack-check -O ReleaseSmall
ASFLAGS += -32 -march=i386

OBJECTS = main.o init.o
BINARY = kernel

.PHONY: all
all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.zig
	zig build-obj $< $(ZIGFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY) *.o.o
