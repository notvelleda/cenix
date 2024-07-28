#!/bin/sh

set -e

CROSS=m68k-unknown-elf-

MAKE_FLAGS="CROSS=$CROSS CC=${CROSS}cc PLATFORM=mac-68000 DEBUG=y"

bmake -C init $MAKE_FLAGS
bmake -C kernel $MAKE_FLAGS

cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
