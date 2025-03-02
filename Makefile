# this variable specifies the prefix for the cross compilation toolchain that will be used to build cenix.
# this variable can be overrided by passing it as an argument to make, as in `make CROSS=cross-toolchain-prefix`
CROSS ?= m68k-unknown-elf-

# this variable specifies the platform that cenix should be built for, in the form of a 2 element tuple separated by a hyphen.
# the first component of the tuple is the machine that will be targetted, and the second component is the architecture that will be targetted.
# these values are used to conditionally compile machine and architecture dependent code, among other things.
# this variable can be overrided by passing it as an argument to make, as in `make PLATFORM=platform-here`
PLATFORM ?= mac-68000

# this variable specifies whether debug mode should be enabled for the build or not, which will enable Cool Things like debug messages.
# however this comes at a cost, as it may increase the size of some programs by a lot which may not be desirable on particularly memory constrained machines.
# this variable can be overrided by passing it as an argument to make, as in `make DEBUG=y`
DEBUG ?= n

# multiple of these variables can be overridden at the same time, as in `make CROSS=something PLATFORM=something-else`

# ==============================================================================

# list of directories to cross compile
DIRECTORIES = core

.PHONY: all native test $(DIRECTORIES) clean

all: $(DIRECTORIES)

# rule for building native utilities as specified in `Makefile.native`
native:
	$(MAKE) -f Makefile.native

# rule for building and running unit tests as specified in `Makefile.test`
test: native
	$(MAKE) -f Makefile.test

# rule for building subdirectories
$(DIRECTORIES): native
	$(MAKE) -C $@ -f Makefile PROJECT_ROOT=.. CROSS="$(CROSS)" PLATFORM="$(PLATFORM)" DEBUG="$(DEBUG)"

clean:
	-rm -r build

# include special rules for this platform if any exist
.if exists(makefiles/platform/$(PLATFORM).mk)
.include "makefiles/platform/$(PLATFORM).mk"
.endif
