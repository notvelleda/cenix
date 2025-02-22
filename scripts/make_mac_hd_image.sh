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
mkdir -p build/initrd/sbin
cp build/vfs_server/vfs_server build/initrd/sbin/
cp build/service_manager/service_manager build/initrd/sbin/

mkdir -p build/initrd/proc
mkdir -p build/initrd/dev

cp build/debug_console/debug_console build/initrd/sbin/debug_console
cp build/initrd_fs/initrd_fs build/initrd/sbin/initrd_fs

cd build/initrd
../jax/jax -cvf ../initrd.jax *
cd ../..

make_in core/process_server $MAKE_FLAGS DEBUG=y
make_in core/kernel $MAKE_FLAGS DEBUG=y # disabling debug for the kernel saves like 9-10k

echo ==== \(bootloader image creation\)
cd tiny-mac-bootloader
mkdir -p fs-contents
cp ../build/kernel/kernel fs-contents/
${CROSS}strip fs-contents/kernel
touch fs-contents/cmdline
rm floppy.img || true
make CROSS=$CROSS AS=${CROSS}as LD=${CROSS}ld floppy.img
