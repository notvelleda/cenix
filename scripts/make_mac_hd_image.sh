#!/bin/sh

# TODO: this script should probably be replaced by a makefile or something eventually but it works fine for now

set -e

CROSS=m68k-unknown-elf-

MAKE_FLAGS="CROSS=$CROSS CC=${CROSS}cc PLATFORM=mac-68000 $@"

make_in() {
    echo ==== $1
    bmake -C $@
}

make_in utils/jax -f Makefile.native $@

make_in core/vfs_server $MAKE_FLAGS DEBUG=y
make_in core/service_manager $MAKE_FLAGS

make_in core/debug_console $MAKE_FLAGS
make_in core/initrd_fs $MAKE_FLAGS

echo ==== \(initrd creation\)
mkdir -p initrd/sbin
cp core/vfs_server/vfs_server initrd/sbin/
cp core/service_manager/service_manager initrd/sbin/

mkdir -p initrd/proc
mkdir -p initrd/dev

cp core/debug_console/debug_console initrd/sbin/debug_console
cp core/initrd_fs/initrd_fs initrd/sbin/initrd_fs

cd initrd
../utils/jax/jax -cvf ../initrd.jax *
cd ..

make_in core/process_server $MAKE_FLAGS DEBUG=y
make_in core/kernel $MAKE_FLAGS DEBUG=y # disabling debug for the kernel saves like 9-10k

echo ==== \(bootloader image creation\)
cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../core/kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img || true
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
