PROJ = hsm

# objs in addition to $(PROJ).o
OBJS =	mgmt-cli.o \
	mgmt-firmware.o \
	mgmt-bootloader.o \
	mgmt-fpga.o \
	mgmt-keystore.o \
	mgmt-masterkey.o \
	mgmt-misc.o \
	mgmt-task.o \
	log.o

BOARD_OBJS += $(TOPLEVEL)/task.o

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
