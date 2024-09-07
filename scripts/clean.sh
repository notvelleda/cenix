#!/bin/sh

bmake -C jax -f Makefile.native clean
bmake -C vfs_server clean
bmake -C service_manager clean
bmake -C debug_console clean
bmake -C initrd_jax_fs clean
bmake -C process_server clean
bmake -C kernel clean
