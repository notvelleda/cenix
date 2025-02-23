# special make rules for the mac-68000 platform, included by the top level makefile when building for this platform

.PHONY: mac-floppy clean-bootloader

all: mac-floppy
clean: clean-bootloader

clean-bootloader:
	$(MAKE) -C tiny-mac-bootloader clean

# rule to build the floppy image for the mac-68000 target
mac-floppy: core
	mkdir -p tiny-mac-bootloader/fs-contents
	cp build/kernel/kernel tiny-mac-bootloader/fs-contents/
	${CROSS}strip tiny-mac-bootloader/fs-contents/kernel
	touch tiny-mac-bootloader/fs-contents/cmdline
	rm tiny-mac-bootloader/floppy.img || true
	$(MAKE) -C tiny-mac-bootloader CROSS="$CROSS" AS="$(CROSS)as" LD="$(CROSS)ld" floppy.img
