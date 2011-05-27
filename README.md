Ultim809 Emulator for MESS
==========================

This is a somewhat complete emulation of the Ultim809 for MESS, the Multi
Emulator Super System. [http://mess.org](http://mess.org)

Supported features:

*  RAM bank switching
*  Video and sound emulation
*  Game controllers (3-button)
*  Loading applications as "cartridges"

Un-emulated features:

*  PS/2 keyboard (MESS's implementation is too buggy)
*  SD card

Building
--------
First, obtain the MESS source from the [official repository.]()

Then:
*  copy `ultim809.c` to `src/mess/drivers/`
*  copy `ultim809.h` to `src/mess/includes/ultim809.h`
*  apply `ultim809_for_mess.diff`, which updates `src/mess/messdriv.c` and
   `src/mess/mess.mak`
*  modify `ultim809.h` line 41 to point to the `checksums.h` file generated by
   the Ultim809 rom build process

Running
-------
The ROM image can be obtained from the `code/rom/` directory of the Ultim809
distribution, it is called `rom.bin`. I haven't tested this in a while, it might
not work.

