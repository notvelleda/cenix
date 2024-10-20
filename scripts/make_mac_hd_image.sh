#!/bin/sh

set -e

CROSS=m68k-unknown-elf-

MAKE_FLAGS="CROSS=$CROSS CC=${CROSS}cc PLATFORM=mac-68000 $@"

bmake -C jax -f Makefile.native $@

bmake -C core/vfs_server $MAKE_FLAGS DEBUG=y
bmake -C core/service_manager $MAKE_FLAGS

bmake -C core/debug_console $MAKE_FLAGS
bmake -C core/initrd_jax_fs $MAKE_FLAGS

mkdir -p initrd/sbin
cp core/vfs_server/vfs_server initrd/sbin/
cp core/service_manager/service_manager initrd/sbin/

mkdir -p initrd/proc
mkdir -p initrd/dev

cp core/debug_console/debug_console initrd/sbin/debug_console
cp core/initrd_jax_fs/initrd_jax_fs initrd/sbin/initrd_jax_fs

cd initrd
../jax/jax -cvf ../initrd.jax *
cd ..

bmake -C core/process_server $MAKE_FLAGS DEBUG=y
bmake -C core/kernel $MAKE_FLAGS DEBUG=y # disabling debug for the kernel saves like 11k

cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../core/kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img || true
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
