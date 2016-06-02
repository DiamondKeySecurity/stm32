TEST = cli-test

OBJS = crc32.o test_sdram.o mgmt-cli.o mgmt-dfu.c mgmt-fpga.c mgmt-misc.c mgmt-show.c mgmt-test.c

CFLAGS += -I$(LIBCLI_DIR)
LIBS += $(LIBCLI_DIR)/libcli.a

all: $(TEST:=.elf)

%.elf: %.o $(BOARD_OBJS) $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $@ -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(SIZE) $*.elf

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.map
