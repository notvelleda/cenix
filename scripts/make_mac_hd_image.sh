#!/bin/sh

set -e

CROSS=m68k-unknown-elf-

make -C kernel CROSS=$CROSS CC=${CROSS}cc LD=${CROSS}ld PLATFORM=mac-68000

cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
