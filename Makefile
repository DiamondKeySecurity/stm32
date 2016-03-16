# Copyright (c) 2015, NORDUnet A/S
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# - Neither the name of the NORDUnet nor the names of its contributors may
#   be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# "stm32-native" projects
SELF-TESTS = fmc-test led-test short-test uart-test fmc-perf
vpath %.c self-test

# apps originally written for unix-like environment
LIBHAL-TESTS = cores test-bus test-trng test-hash test-aes-key-wrap test-pbkdf2 test-ecdsa test-rsa test-rpc_server
vpath %.c libhal/tests libhal/utils

# absolute path, because we're going to be passing -I cflags to sub-makes
TOPLEVEL = $(shell pwd)

# Location of the Libraries folder from the STM32F0xx Standard Peripheral Library
STD_PERIPH_LIB = $(TOPLEVEL)/Drivers

# linker script
LDSCRIPT = $(TOPLEVEL)/Device/ldscripts/stm32f429bitx.ld

# board-specific objects, to link into every project
BOARD_OBJS = stm32f4xx_hal_msp.o stm32f4xx_it.o stm-fmc.o stm-init.o stm-uart.o \
	Device/startup_stm32f429xx.o Device/system_stm32f4xx.o

# a few objects for libhal/test projects
LIBC_OBJS = syscalls.o printf.o gettimeofday.o

LIBS = $(STD_PERIPH_LIB)/libstmf4.a libhal/libhal.a thirdparty/libtfm/libtfm.a

# cross-building tools
PREFIX=arm-none-eabi-
export CC=$(PREFIX)gcc
export AS=$(PREFIX)as
export AR=$(PREFIX)ar
export OBJCOPY=$(PREFIX)objcopy
export OBJDUMP=$(PREFIX)objdump
export SIZE=$(PREFIX)size

# whew, that's a lot of cflags
CFLAGS  = -ggdb -O2 -Wall -Warray-bounds #-Wextra
CFLAGS += -mcpu=cortex-m4 -mthumb -mlittle-endian -mthumb-interwork
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -DUSE_STDPERIPH_DRIVER -DSTM32F4XX -DSTM32F429xx
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wl,--gc-sections
CFLAGS += -std=c99
CFLAGS += -I $(TOPLEVEL) -I $(STD_PERIPH_LIB)
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F4xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include
CFLAGS += -I $(STD_PERIPH_LIB)/STM32F4xx_HAL_Driver/Inc
CFLAGS += -I libhal
export CFLAGS

all: lib self-test libhal-tests

init:
	git submodule update --init --recursive

lib: $(LIBS)

$(STD_PERIPH_LIB)/libstmf4.a:
	$(MAKE) -C $(STD_PERIPH_LIB)

thirdparty/libtfm/libtfm.a:
	$(MAKE) -C thirdparty/libtfm PREFIX=$(PREFIX)

libhal/libhal.a: thirdparty/libtfm/libtfm.a
	$(MAKE) -C libhal IO_BUS=fmc RPC_SERVER=yes RPC_TRANSPORT=serial KS=volatile libhal.a

self-test: $(SELF-TESTS:=.elf)

%.elf: %.o $(BOARD_OBJS) $(STD_PERIPH_LIB)/libstmf4.a
	$(CC) $(CFLAGS) $^ -o $@ -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

libhal-tests: $(LIBHAL-TESTS:=.bin)

# .mo extension for files with main() that need to be wrapped as __main()
%.mo: %.c
	$(CC) -c $(CFLAGS) -Dmain=__main -o $@ $<

%.bin: %.mo main.o $(BOARD_OBJS) $(LIBC_OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $*.elf -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

# don't automatically delete objects, to avoid a lot of unnecessary rebuilding
.SECONDARY: $(BOARD_OBJS) $(LIBC_OBJS)

clean:
	find ./ -name '*~' | xargs rm -f
	rm -f $(BOARD_OBJS) $(LIBC_OBJS) *.o *.mo
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst

distclean: clean
	$(MAKE) -C $(STD_PERIPH_LIB) clean
	$(MAKE) -C thirdparty/libtfm clean
	$(MAKE) -C libhal clean
