PROJ = hsm

# objs in addition to $(PROJ).o
OBJS = crc32.o \
	mgmt-cli.o \
	mgmt-firmware.c \
	mgmt-bootloader.c \
	mgmt-fpga.c \
	mgmt-keystore.c \
	mgmt-masterkey.c \
	mgmt-misc.c \
	mgmt-show.c

BOARD_OBJS = \
	$(TOPLEVEL)/stm-init.o \
	$(TOPLEVEL)/stm-fmc.o \
	$(TOPLEVEL)/stm-uart.o \
	$(TOPLEVEL)/syscalls.o \
	$(TOPLEVEL)/stm-rtc.o \
	$(TOPLEVEL)/spiflash_n25q128.o \
	$(TOPLEVEL)/stm-fpgacfg.o \
	$(TOPLEVEL)/stm-keystore.o \
	$(TOPLEVEL)/stm-sdram.o \
	$(TOPLEVEL)/stm-flash.o \
	$(BOARD_DIR)/TOOLCHAIN_GCC_ARM/startup_stm32f429xx_rtos.o \
	$(BOARD_DIR)/system_stm32f4xx.o \
	$(BOARD_DIR)/stm32f4xx_hal_msp.o \
	$(BOARD_DIR)/stm32f4xx_it_rtos.o

ifndef CRYPTECH_ROOT
  CRYPTECH_ROOT := $(abspath ../../../..)
endif

CFLAGS += -I$(LIBHAL_SRC)
CFLAGS += -I$(LIBCLI_SRC)
CFLAGS += -I$(RTOS_DIR)/rtos -I$(RTOS_DIR)/rtx/TARGET_CORTEX_M

LIBS += $(LIBHAL_BLD)/libhal.a $(LIBTFM_BLD)/libtfm.a
LIBS += $(LIBCLI_BLD)/libcli.a
LIBS += $(RTOS_DIR)/librtos.a

all: $(PROJ:=.elf)

%.elf: %.o $(BOARD_OBJS) $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $@ -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(SIZE) $*.elf

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.map
