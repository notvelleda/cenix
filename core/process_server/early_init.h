#pragma once

/// \brief runs early initialization steps necessary to get the system into a functional enough state to properly run programs.
///
/// the 1st stage of early init (i.e. this function) is responsible for the following:
///  - starting the VFS server
///  - mounting the initial ramdisk filesystem (initrd) in the root directory
///  - setting up initial sources of stdin, stdout, and stderr to be used until proper ones are found later on in the boot process (currently unimplemented)
///  - mounting /proc (this is a responsibility of the process server itself, however this is done as an early init step) (currently unimplemented)
///  - transferring control to the 2nd stage of early init (which will start the device manager, mount the actual root filesystem, etc.) (currently unimplemented)
///
/// the rationale for this being in the process server is because if the first stage of early init was in a standalone binary it would have to be very closely linked to the process server anyway,
/// so just doing it here is the simplest and easiest option
void early_init(void);
