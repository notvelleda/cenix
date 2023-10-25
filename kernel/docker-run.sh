#!/bin/sh

set -e
docker build -t cenix-buildenv .
docker run --user $(id -u):$(id -u) --mount type=bind,source="$(pwd)",target=/cenix --workdir /cenix -t cenix-buildenv make
echo "(ctrl+c to exit)"
qemu-system-i386 -cpu 486 -device isa-debug-exit -kernel kernel -serial stdio -display none
