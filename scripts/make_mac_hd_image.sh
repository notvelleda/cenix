#!/bin/sh

set -e

CROSS=m68k-unknown-elf-

MAKE_FLAGS="CROSS=$CROSS CC=${CROSS}cc PLATFORM=mac-68000 $@"

bmake -C jax -f Makefile.native $@

bmake -C vfs_server $MAKE_FLAGS DEBUG=y
bmake -C service_manager $MAKE_FLAGS

bmake -C debug_console $MAKE_FLAGS
bmake -C initrd_jax_fs $MAKE_FLAGS

mkdir -p initrd/sbin
cp vfs_server/vfs_server initrd/sbin/
cp service_manager/service_manager initrd/sbin/

mkdir -p initrd/proc
mkdir -p initrd/dev

cp debug_console/debug_console initrd/sbin/debug_console
cp initrd_jax_fs/initrd_jax_fs initrd/sbin/initrd_jax_fs

cd initrd
../jax/jax -cvf ../initrd.jax *
cd ..

bmake -C process_server $MAKE_FLAGS DEBUG=y
bmake -C kernel $MAKE_FLAGS DEBUG=y # disabling debug for the kernel saves like 11k

cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img || true
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
