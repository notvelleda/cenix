#!/bin/sh

set -e
make
echo "(ctrl+c to exit)"
qemu-system-i386 -cpu 486 -device isa-debug-exit -kernel kernel -serial stdio -display none
