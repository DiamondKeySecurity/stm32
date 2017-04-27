PROJ = hsm

# objs in addition to $(PROJ).o
OBJS =	mgmt-cli.o \
	mgmt-firmware.c \
	mgmt-bootloader.c \
	mgmt-fpga.c \
	mgmt-keystore.c \
	mgmt-masterkey.c \
	mgmt-misc.c \
	mgmt-task.c \
	log.o

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
	$(BOARD_DIR)/TOOLCHAIN_GCC_ARM/startup_stm32f429xx.o \
	$(BOARD_DIR)/system_stm32f4xx.o \
	$(BOARD_DIR)/stm32f4xx_hal_msp.o \
	$(BOARD_DIR)/stm32f4xx_it.o \
	$(TOPLEVEL)/task.o

CFLAGS += -DNUM_RPC_TASK=4

CFLAGS += -I$(LIBHAL_SRC)
CFLAGS += -I$(LIBCLI_SRC)

LIBS += $(LIBHAL_BLD)/libhal.a $(LIBTFM_BLD)/libtfm.a
LIBS += $(LIBCLI_BLD)/libcli.a

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
