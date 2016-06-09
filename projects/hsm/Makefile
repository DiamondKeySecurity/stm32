PROJ = hsm

# objs in addition to $(PROJ).o
OBJS = 

CFLAGS += -I $(LIBHAL_DIR)

LIBS += $(LIBHAL_DIR)/libhal.a $(LIBTFM_DIR)/libtfm.a

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
