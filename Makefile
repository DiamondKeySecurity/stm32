PROJS = fmc-test led-test short-test uart-test

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

CC=arm-none-eabi-gcc
AS=arm-none-eabi-as
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-objdump
SIZE=arm-none-eabi-size

CFLAGS  = -ggdb -O2 -Wall -Wextra -Warray-bounds
CFLAGS += -mcpu=cortex-m4 -mthumb -mlittle-endian -mthumb-interwork
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += $(STDPERIPH_SETTINGS)
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wl,--gc-sections

###################################################

vpath %.c src
vpath %.a $(STD_PERIPH_LIB)

CFLAGS += -I include -I $(STD_PERIPH_LIB) -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F4xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include -I $(STD_PERIPH_LIB)/STM32F4xx_HAL_Driver/Inc

OBJS = $(patsubst %.s,%.o, $(patsubst %.c,%.o, $(SRCS)))

###################################################

.PHONY: lib proj

all: lib proj

lib:
	$(MAKE) -C $(STD_PERIPH_LIB) STDPERIPH_SETTINGS="$(STDPERIPH_SETTINGS) -I $(PWD)/include"

proj: $(PROJS:=.elf)

%.elf: %.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -L$(STD_PERIPH_LIB) -lstmf4 -L$(LDSCRIPT_INC) -T$(MCU_LINKSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

clean:
	find ./ -name '*~' | xargs rm -f
	rm -f $(OBJS)
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst

distclean: clean
	$(MAKE) -C $(STD_PERIPH_LIB) clean
