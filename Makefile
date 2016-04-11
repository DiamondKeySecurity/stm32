# Copyright (c) 2015-2016, NORDUnet A/S
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

# absolute path, because we're going to be passing -I cflags to sub-makes
export TOPLEVEL = $(shell pwd)

# Location of the Libraries folder from the STM32F0xx Standard Peripheral Library
STD_PERIPH_LIB = $(TOPLEVEL)/Drivers
export LIBS = $(STD_PERIPH_LIB)/libstmf4.a

# linker script
export LDSCRIPT = $(TOPLEVEL)/Device/ldscripts/stm32f429bitx.ld

# board-specific objects, to link into every project
export BOARD_OBJS = $(TOPLEVEL)/stm32f4xx_hal_msp.o \
	$(TOPLEVEL)/stm32f4xx_it.o \
	$(TOPLEVEL)/stm-fmc.o \
	$(TOPLEVEL)/stm-init.o \
	$(TOPLEVEL)/stm-uart.o \
	$(TOPLEVEL)/syscalls.o \
	$(TOPLEVEL)/Device/startup_stm32f429xx.o \
	$(TOPLEVEL)/Device/system_stm32f4xx.o

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
CFLAGS += -D__CORTEX_M4 -DTARGET_STM -DTARGET_STM32F4 -DTARGET_STM32F429ZI -DTOOLCHAIN_GCC -D__FPU_PRESENT=1
CFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections
CFLAGS += -std=c99
CFLAGS += -I $(TOPLEVEL)
CFLAGS += -I $(STD_PERIPH_LIB)
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F4xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include
CFLAGS += -I $(STD_PERIPH_LIB)/STM32F4xx_HAL_Driver/Inc
export CFLAGS

all: board-test libhal-test

init:
	git submodule update --init --recursive

$(STD_PERIPH_LIB)/libstmf4.a:
	$(MAKE) -C $(STD_PERIPH_LIB)

board-test: $(BOARD_OBJS) $(LIBS)
	$(MAKE) -C projects/board-test

LIBS_DIR = $(TOPLEVEL)/libraries

export LIBTFM_DIR = $(LIBS_DIR)/thirdparty/libtfm

$(LIBTFM_DIR)/libtfm.a:
	$(MAKE) -C $(LIBTFM_DIR) PREFIX=$(PREFIX)

export LIBHAL_DIR = $(LIBS_DIR)/libhal

$(LIBHAL_DIR)/libhal.a: $(LIBTFM_DIR)/libtfm.a
#	$(MAKE) -C $(LIBHAL_DIR) RPC_CLIENT=local IO_BUS=fmc KS=volatile libhal.a
	$(MAKE) -C $(LIBHAL_DIR) IO_BUS=fmc RPC_SERVER=yes RPC_TRANSPORT=serial KS=volatile libhal.a

libhal-test: $(BOARD_OBJS) $(LIBS) $(LIBHAL_DIR)/libhal.a
	$(MAKE) -C projects/libhal-test

# don't automatically delete objects, to avoid a lot of unnecessary rebuilding
.SECONDARY: $(BOARD_OBJS)

.PHONY: board-test libhal-test

clean:
	rm -f $(BOARD_OBJS)
	$(MAKE) -C projects/board-test clean
	$(MAKE) -C projects/libhal-test clean

distclean: clean
	$(MAKE) -C $(STD_PERIPH_LIB) clean
	$(MAKE) -C $(LIBHAL_DIR) clean
	$(MAKE) -C $(LIBTFM_DIR) clean
