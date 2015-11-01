SELF-TESTS = fmc-test led-test short-test uart-test fmc-perf

LIBHAL-TESTS = cores test-bus test-hash test-aes-key-wrap test-pbkdf2 test-ecdsa #test-rsa

# put your *.o targets here, make should handle the rest!
SRCS = stm32f4xx_hal_msp.c stm32f4xx_it.c stm-fmc.c stm-init.c stm-uart.c

TOPLEVEL=.

# Location of the Libraries folder from the STM32F0xx Standard Peripheral Library
STD_PERIPH_LIB ?= $(TOPLEVEL)/Drivers

# Location of the linker scripts
LDSCRIPT_INC ?= $(TOPLEVEL)/Device/ldscripts

# MCU selection parameters
#
STDPERIPH_SETTINGS ?= -DUSE_STDPERIPH_DRIVER -DSTM32F4XX -DSTM32F429xx
#
# For the dev-bridge rev01 board, use stm32f429bitx.ld.
MCU_LINKSCRIPT ?= stm32f429bitx.ld

# add startup file to build
#
# For the dev-bridge rev01 board, use startup_stm32f429xx.s.
SRCS += $(TOPLEVEL)/Device/startup_stm32f429xx.s
SRCS += $(TOPLEVEL)/Device/system_stm32f4xx.c

# that's it, no need to change anything below this line!

###################################################

PREFIX=arm-none-eabi-
export CC=$(PREFIX)gcc
export AS=$(PREFIX)as
export AR=$(PREFIX)ar
export OBJCOPY=$(PREFIX)objcopy
export OBJDUMP=$(PREFIX)objdump
export SIZE=$(PREFIX)size

#CFLAGS  = -ggdb -O2 -Wall -Wextra -Warray-bounds
CFLAGS  = -ggdb -O2 -Wall -Warray-bounds
CFLAGS += -mcpu=cortex-m4 -mthumb -mlittle-endian -mthumb-interwork
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += $(STDPERIPH_SETTINGS)
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wl,--gc-sections
CFLAGS += -std=c99

###################################################

vpath %.c src self-test
vpath %.a $(STD_PERIPH_LIB)

CFLAGS += -I include -I $(STD_PERIPH_LIB) -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F4xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include -I $(STD_PERIPH_LIB)/STM32F4xx_HAL_Driver/Inc

OBJS = $(patsubst %.s,%.o, $(patsubst %.c,%.o, $(SRCS)))

###################################################

.PHONY: lib self-test

LIBS = $(STD_PERIPH_LIB)/libstmf4.a libhal/libhal.a thirdparty/libtfm/libtfm.a

all: lib self-test libhal-tests

init:
	git submodule update --init --recursive

lib: $(LIBS)

export CFLAGS

$(STD_PERIPH_LIB)/libstmf4.a:
	$(MAKE) -C $(STD_PERIPH_LIB) STDPERIPH_SETTINGS="$(STDPERIPH_SETTINGS) -I $(PWD)/include"

thirdparty/libtfm/libtfm.a:
	$(MAKE) -C thirdparty/libtfm PREFIX=$(PREFIX)

libhal/libhal.a: hal_io_fmc.o thirdparty/libtfm/libtfm.a
	$(MAKE) -C libhal IO_OBJ=../hal_io_fmc.o libhal.a

self-test: $(SELF-TESTS:=.elf)

%.elf: %.o $(OBJS) $(STD_PERIPH_LIB)/libstmf4.a
	$(CC) $(CFLAGS) $^ -o $@ -L$(LDSCRIPT_INC) -T$(MCU_LINKSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

libhal-tests: $(LIBHAL-TESTS:=.bin)

vpath %.c libhal/tests
CFLAGS += -I libhal

# .mo extension for files with main() that need to be wrapped as __main()
%.mo: %.c
	$(CC) -c $(CFLAGS) -Dmain=__main -o $@ $<

vpath %.c libc libhal/utils
%.bin: %.mo main.o syscalls.o printf.o gettimeofday.o $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $*.elf -L$(LDSCRIPT_INC) -T$(MCU_LINKSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

.SECONDARY: $(OBJS) *.mo main.o syscalls.o

clean:
	find ./ -name '*~' | xargs rm -f
	rm -f $(OBJS) *.o *.mo
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst

distclean: clean
	$(MAKE) -C $(STD_PERIPH_LIB) clean
	$(MAKE) -C thirdparty/libtfm clean
	$(MAKE) -C libhal clean

