TEST = thread-test semaphore-test mutex-test

CFLAGS += -I$(RTOS_DIR)/rtos -I$(RTOS_DIR)/rtx/TARGET_CORTEX_M

LIBS += $(RTOS_DIR)/librtos.a

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
