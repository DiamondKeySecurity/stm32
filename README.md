STM32 software for dev-bridge/Alpha board
=========================================

The dev-bridge board is a daughterboard for the Novena, which talks to the
Novena's FPGA through the high-speed expansion connector.

The Alpha board is a stand-alone board with an Artix-7 FPGA, a STM32 Cortex-M4
microcontroller, two USB interfaces etc.

See user/ft/stm32-dev-bridge/hardware/rev01 for schematics of the bridge
board. There will be more information on the wiki shortly.


Copyrights
==========

The license for all work done on this in the CrypTech project is a
3-clause BSD license (see LICENSE.txt for details). Some files have
been generated using the STMicroelectronics initialization code
generator STM32CubeMX and thus have additional copyright header(s).

The "Noise generator" and "Amplifier" parts of the circuit diagram are
copied from Benedikt Stockebrand's ARRGH project. ARRGH copyright
statement is included in LICENSE.txt.

A stripped down copy of the ARM CMSIS library version 3.20 is included
in the Drivers/CMSIS/ directory. Unused parts (and documentation etc.)
have been removed, but every attempt have been made to keep any
licensing information intact. See in particular the file
Drivers/CMSIS/CMSIS END USER LICENCE AGREEMENT.pdf.

A full copy of the STM32F4xx HAL Drivers is included in the
Drivers/STM32F4xx_HAL_Driver/ directory.


Building
========

The following packages need to be installed (on Ubuntu 14.04):

  apt-get install gcc-arm-none-eabi gdb-arm-none-eabi openocd

To build the source code, issue "make" from the top level directory
(where this file is). The first time, this will build the complete STM
CMSIS library. A subsequent "make clean" will *not* clean away the CMSIS
library, but a "make distclean" will.


Installing
==========

Do "bin/flash-target" from the top level directory (where this file is)
to flash a built image into the microcontroller. See the section ST-LINK
below for information about the actual hardware programming device needed.

Example loading the bootloader and the led-test firmware to get some LEDs
flashing:

  $ make bootloader board-test
  $ ./bin/flash-target projects/board-test/led-test
  $ ./bin/flash-target projects/bootloader/bootloader

At this point, the STM32 will reset into the bootloader which flashes the
blue LED five times in one second, and then execution of the LED test
firmware will begin. The LED test firmware will flash the green, yellow,
red and blue LEDs in order until the end of time.

Once the bootloader is installed, regular firmware can be loaded without
an ST-LINK cable like this:

  $ ./bin/dfu projects/board-test/led-test.bin

Then reboot the Alpha board.


ST-LINK
=======
To program the MCU, an ST-LINK adapter is used. The cheapest way to get
one is to buy an evaluation board with an ST-LINK integrated, and pinouts
to program external chips. This should work with any evaluation board from
STM; we have tested with STM32F4DISCOVERY (with ST-LINK v2.0) and
NUCLEO-F411RE (with ST-LINK v2.1).

The ST-LINK programming pins is called J1 and is near the CrypTech logo
printed on the circuit board. The pin-outs is shown on the circuit board
(follow the thin white line from J1 to the white box with STM32_SWD
written in it). From left to right, the pins are

  3V3, CLK, GND, I/O, NRST and N/C

This matches the pin-out on the DISCO and NUCLEO boards we have tried.

First remove the pair of ST-LINK jumpers (CN4 on the DISCO, CN2 on the
NUCLEO). Then find the 6-pin SWD header on the left of the STM board (CN2
on the DISCO, CN4 on the NUCLEO), and connect them to the Alpha board:

    NUCLEO / DISCO        CRYPTECH ALPHA
    --------------        --------------
* 1 VDD_TARGET        <-> 3V3
* 2 SWCLK / T_JTCK    <-> CLK
* 3 GND               <-> GND
* 4 SWDIO / T_JTMS    <-> IO
* 5 T_NRST / NRST     <-> NRST

N/C (pin 6) means Not Connected.

The Alpha board should be powered on before attempting to flash it.


Debugging the firmware
======================

This site shows several ways to use various debuggers to debug the
firmware in an STM32:

  http://fun-tech.se/stm32/OpenOCD/gdb.php

There is a shell script called 'bin/debug' that starts an OpenOCD server
and GDB. Example:

  $ ./bin/debug projects/board-test/led-test

Once in GDB, issue "monitor reset halt" to reset the STM32 before debugging.

Remember that the first code to run will be the bootloader, but if you do
e.g. "break main" and "continue" you will end up in led-test main() after
the bootloader has jumped there.
