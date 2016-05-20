STM32 software for dev-bridge board
===================================

The dev-bridge board is a daughterboard for the Novena, which talks to the
Novena's FPGA through the high-speed expansion connector.

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


ST-LINK
=======
To program the MCU, an ST-LINK adapter is used. The cheapest way to get
one is to buy an evaluation board with an ST-LINK integrated, and pinouts
to program external chips. This should work with any evaluation board from
STM; we have tested with STM32F4DISCOVERY (with ST-LINK v2.0) and 
NUCLEO-F411RE (with ST-LINK v2.1).

The ST-LINK programming pins are the 1+4 throughole pads above the ARM
on the circuit board. See the schematics for details, but the pinout
from left to right (1, space, 4) of rev01 is

  NRST, space, CLK, IO, GND, VCC

First remove the pair of ST-LINK jumpers (CN4 on the DISCO, CN2 on the
NUCLEO). Then find the 6-pin SWD header on the left of the STM board (CN2
on the DISCO, CN4 on the NUCLEO), and connect them to the dev-bridge
board:

* 5 T_NRST <-> NRST
* 2 T_JTCK <-> CLK
* 4 T_JTMS <-> IO
* 3 GND <-> GND

The dev-bridge board should be connected to the Novena and powered on
before attempting to flash it.


Debugging the firmware
======================

This site shows several ways to use various debuggers to debug the
firmware in an STM32:

  http://fun-tech.se/stm32/OpenOCD/gdb.php

I've only managed to get the most basic text line gdb to work,
something along these lines:

1) Start OpenOCD server (with a configuration file for your type of ST-LINK
   adapter)

   $ openocd -f /usr/share/openocd/scripts/board/stm32f4discovery.cfg

2) Connect to the OpenOCD server and re-flash already compiled firmware:

   $ telnet localhost 4444
   reset halt
   flash probe 0
   stm32f2x mass_erase 0
   flash write_bank 0 /path/to/main.bin 0
   reset halt

3) Start GDB and have it connect to the OpenOCD server:

   $ arm-none-eabi-gdb --eval-command="target remote localhost:3333" main.elf

