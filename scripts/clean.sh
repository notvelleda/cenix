#!/bin/sh

bmake -C jax -f Makefile.native clean
bmake -C core/vfs_server clean
bmake -C core/service_manager clean
bmake -C core/debug_console clean
bmake -C core/initrd_jax_fs clean
bmake -C core/process_server clean
bmake -C core/kernel clean
