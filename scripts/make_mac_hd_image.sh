#!/bin/sh

set -e

CROSS=m68k-unknown-elf-

MAKE_FLAGS="CROSS=$CROSS CC=${CROSS}cc PLATFORM=mac-68000"

bmake -C init $MAKE_FLAGS DEBUG=y
bmake -C kernel $MAKE_FLAGS DEBUG=y # disabling debug for the kernel saves like 18k

cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
