PROJ = hsm

SRCS = main.c

OBJS = $(SRCS:.c=.o)

CFLAGS += -I $(LIBHAL_DIR)

LIBS += $(LIBHAL_DIR)/libhal.a $(LIBTFM_DIR)/libtfm.a

all: $(PROJ:=.elf)

$(PROJ).elf: $(OBJS) $(BOARD_OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $@ -T$(LDSCRIPT) -g -Wl,-Map=$(PROJ).map
	$(OBJCOPY) -O binary $(PROJ).elf $(PROJ).bin
	$(SIZE) $(PROJ).elf

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.bin
	rm -f *.map
