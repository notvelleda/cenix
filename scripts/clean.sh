#!/bin/sh

bmake -C core/debug_console clean
bmake -C core/initrd_fs clean
bmake -C core/kernel clean
bmake -C core/process_server clean
bmake -C core/service_manager clean
bmake -C core/vfs_server clean
bmake -C utils/jax -f Makefile.native clean
rm -r initrd
rm initrd.jax
