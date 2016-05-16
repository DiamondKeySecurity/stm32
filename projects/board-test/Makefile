TEST = led-test short-test uart-test fmc-test fmc-perf fmc-probe rtc-test

all: $(TEST:=.elf)

%.elf: %.o $(BOARD_OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $@ -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst
