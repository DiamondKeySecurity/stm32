TEST = ekermit-test

OBJS = ../libhal-test/printf.o

BOARD_OBJS += $(TOPLEVEL)/sdram-malloc.o

CFLAGS += -I$(EKERMIT_DIR)

LIBS += $(EKERMIT_DIR)/libekermit.a

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
