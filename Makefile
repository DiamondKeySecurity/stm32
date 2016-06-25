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

# absolute path, because we're going to be passing things to sub-makes
export TOPLEVEL = $(abspath .)
export CRYPTECH_ROOT = $(abspath ../..)

# define board: dev-bridge or alpha
BOARD = TARGET_CRYPTECH_ALPHA

export LIBS_DIR = $(TOPLEVEL)/libraries
export MBED_DIR = $(LIBS_DIR)/mbed
export CMSIS_DIR = $(MBED_DIR)/targets/cmsis/TARGET_STM/TARGET_STM32F4
export BOARD_DIR = $(CMSIS_DIR)/$(BOARD)
export RTOS_DIR = $(MBED_DIR)/rtos

export LIBHAL_SRC = $(CRYPTECH_ROOT)/sw/libhal
export LIBHAL_BLD = $(LIBS_DIR)/libhal

export LIBCLI_SRC = $(CRYPTECH_ROOT)/user/ft/libcli
export LIBCLI_BLD = $(LIBS_DIR)/libcli

export LIBTFM_SRC = $(CRYPTECH_ROOT)/sw/thirdparty/libtfm
export LIBTFM_BLD = $(LIBS_DIR)/libtfm

export LIBS = $(MBED_DIR)/libstmf4.a

# linker script
export LDSCRIPT = $(BOARD_DIR)/TOOLCHAIN_GCC_ARM/STM32F429BI.ld
export BOOTLOADER_LDSCRIPT = $(BOARD_DIR)/TOOLCHAIN_GCC_ARM/STM32F429BI_bootloader.ld

# board-specific objects, to link into every project
BOARD_OBJS = \
	$(TOPLEVEL)/stm-init.o \
	$(TOPLEVEL)/stm-fmc.o \
	$(TOPLEVEL)/stm-uart.o \
	$(TOPLEVEL)/syscalls.o \
	$(BOARD_DIR)/TOOLCHAIN_GCC_ARM/startup_stm32f429xx.o \
	$(BOARD_DIR)/system_stm32f4xx.o \
	$(BOARD_DIR)/stm32f4xx_hal_msp.o \
	$(BOARD_DIR)/stm32f4xx_it.o
ifeq (${BOARD},TARGET_CRYPTECH_ALPHA)
BOARD_OBJS += \
	$(TOPLEVEL)/stm-rtc.o \
	$(TOPLEVEL)/spiflash_n25q128.o \
	$(TOPLEVEL)/stm-fpgacfg.o \
	$(TOPLEVEL)/stm-keystore.o \
	$(TOPLEVEL)/stm-sdram.o \
	$(TOPLEVEL)/stm-flash.o
endif
export BOARD_OBJS

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
CFLAGS += -D__CORTEX_M4 -DTARGET_STM -DTARGET_STM32F4 -DTARGET_STM32F429ZI -DTOOLCHAIN_GCC -D__FPU_PRESENT=1 -D$(BOARD)
CFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections
CFLAGS += -std=c99
CFLAGS += -I$(TOPLEVEL)
CFLAGS += -I$(MBED_DIR)/api
CFLAGS += -I$(MBED_DIR)/targets/cmsis
CFLAGS += -I$(MBED_DIR)/targets/cmsis/TARGET_STM/TARGET_STM32F4
CFLAGS += -I$(MBED_DIR)/targets/cmsis/TARGET_STM/TARGET_STM32F4/$(BOARD)
CFLAGS += -I$(MBED_DIR)/targets/hal/TARGET_STM/TARGET_STM32F4
CFLAGS += -I$(MBED_DIR)/targets/hal/TARGET_STM/TARGET_STM32F4/$(BOARD)
export CFLAGS

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.S
	$(CC) $(CFLAGS) -c -o $@ $<

all: board-test cli-test libhal-test hsm bootloader

$(MBED_DIR)/libstmf4.a:
	$(MAKE) -C $(MBED_DIR)

board-test: $(BOARD_OBJS) $(LIBS)
	$(MAKE) -C projects/board-test

cli-test: $(BOARD_OBJS) $(LIBS) $(LIBCLI_BLD)/libcli.a $(LIBHAL_BLD)/libhal.a $(RTOS_DIR)/librtos.a
	$(MAKE) -C projects/cli-test

$(RTOS_DIR)/librtos.a:
	$(MAKE) -C $(RTOS_DIR)

rtos-test: $(RTOS_OBJS) $(LIBS) $(RTOS_DIR)/librtos.a
	$(MAKE) -C projects/rtos-test

$(LIBTFM_BLD)/libtfm.a:
	$(MAKE) -C $(LIBTFM_BLD) PREFIX=$(PREFIX)

$(LIBHAL_BLD)/libhal.a: $(LIBTFM_BLD)/libtfm.a
	$(MAKE) -C $(LIBHAL_BLD) IO_BUS=fmc RPC_MODE=server RPC_TRANSPORT=serial KS=flash libhal.a

$(LIBCLI_BLD)/libcli.a:
	$(MAKE) -C $(LIBCLI_BLD)

libhal-test: $(BOARD_OBJS) $(LIBS) $(LIBHAL_BLD)/libhal.a
	$(MAKE) -C projects/libhal-test

hsm: $(BOARD_OBJS) $(LIBS) $(LIBHAL_BLD)/libhal.a $(RTOS_DIR)/librtos.a $(LIBCLI_BLD)/libcli.a
	$(MAKE) -C projects/hsm

bootloader: $(BOARD_OBJS) $(LIBS)
	$(MAKE) -C projects/bootloader

# don't automatically delete objects, to avoid a lot of unnecessary rebuilding
.SECONDARY: $(BOARD_OBJS)

.PHONY: board-test rtos-test libhal-test cli-test hsm bootloader

clean:
	rm -f $(BOARD_OBJS)
	$(MAKE) -C projects/board-test clean
	$(MAKE) -C projects/cli-test clean
	$(MAKE) -C projects/rtos-test clean
	$(MAKE) -C projects/libhal-test clean
	$(MAKE) -C projects/hsm clean

distclean: clean
	$(MAKE) -C $(MBED_DIR) clean
	$(MAKE) -C $(RTOS_DIR) clean
	$(MAKE) -C $(LIBHAL_BLD) clean
	$(MAKE) -C $(LIBTFM_BLD) clean
	$(MAKE) -C $(LIBCLI_BLD) clean
