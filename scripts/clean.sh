#!/bin/sh

bmake -C vfs_server clean
bmake -C service_manager clean
bmake -C debug_console clean
bmake -C initrd_tar_fs clean
bmake -C process_server clean
bmake -C kernel clean
